/* Simple subrs.
   Copyright (C) 2019 Free Software Foundation, Inc.

   This file is part of libctf.

   libctf is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not see
   <http://www.gnu.org/licenses/>.  */

#include <ctf-impl.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#include <sys/types.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

static size_t _PAGESIZE _libctf_unused_;
int _libctf_version = CTF_VERSION;	      /* Library client version.  */
int _libctf_debug = 0;			      /* Debugging messages enabled.  */

_libctf_malloc_ void *
ctf_data_alloc (size_t size)
{
  void *ret;

#ifdef HAVE_MMAP
  if (_PAGESIZE == 0)
    _PAGESIZE = sysconf(_SC_PAGESIZE);

  if (size > _PAGESIZE)
    {
      ret = mmap (NULL, size, PROT_READ | PROT_WRITE,
		  MAP_PRIVATE | MAP_ANON, -1, 0);
      if (ret == MAP_FAILED)
	ret = NULL;
    }
  else
    ret = calloc (1, size);
#else
  ret = calloc (1, size);
#endif
  return ret;
}

void
ctf_data_free (void *buf, size_t size _libctf_unused_)
{
#ifdef HAVE_MMAP
  /* Must be the same as the check in ctf_data_alloc().  */

  if (size > _PAGESIZE)
    (void) munmap (buf, size);
  else
    free (buf);
#else
  free (buf);
#endif
}

/* Private, read-only mmap from a file, with fallback to copying.

   No handling of page-offset issues at all: the caller must allow for that. */

_libctf_malloc_ void *
ctf_mmap (size_t length, size_t offset, int fd)
{
  void *data;

#ifdef HAVE_MMAP
  data = mmap (NULL, length, PROT_READ, MAP_PRIVATE, fd, offset);
  if (data == MAP_FAILED)
    data = NULL;
#else
  if ((data = malloc (length)) != NULL)
    {
      if (ctf_pread (fd, data, length, offset) <= 0)
	{
	  free (data);
	  data = NULL;
	}
    }
#endif
  return data;
}

void
ctf_munmap (void *buf, size_t length _libctf_unused_)
{
#ifdef HAVE_MMAP
  (void) munmap (buf, length);
#else
  free (buf);
#endif
}

void
ctf_data_protect (void *buf _libctf_unused_, size_t size _libctf_unused_)
{
#ifdef HAVE_MMAP
  /* Must be the same as the check in ctf_data_alloc().  */

  if (size > _PAGESIZE)
    (void) mprotect (buf, size, PROT_READ);
#endif
}

_libctf_malloc_ void *
ctf_alloc (size_t size)
{
  return (malloc (size));
}

void
ctf_free (void *buf)
{
  free (buf);
}

ssize_t
ctf_pread (int fd, void *buf, ssize_t count, off_t offset)
{
  ssize_t len;
  size_t acc = 0;
  char *data = (char *) buf;

#ifdef HAVE_PREAD
  while (count > 0)
    {
      errno = 0;
      if (((len = pread (fd, data, count, offset)) < 0) &&
	  errno != EINTR)
	  return len;
      if (errno == EINTR)
	continue;

      acc += len;
      if (len == 0)				/* EOF.  */
	return acc;

      count -= len;
      offset += len;
      data += len;
    }
  return acc;
#else
  off_t orig_off;

  if ((orig_off = lseek (fd, 0, SEEK_CUR)) < 0)
    return -1;
  if ((lseek (fd, offset, SEEK_SET)) < 0)
    return -1;

  while (count > 0)
    {
      errno = 0;
      if (((len = read (fd, data, count)) < 0) &&
	  errno != EINTR)
	  return len;
      if (errno == EINTR)
	continue;

      acc += len;
      if (len == 0)				/* EOF.  */
	break;

      count -= len;
      data += len;
    }
  if ((lseek (fd, orig_off, SEEK_SET)) < 0)
    return -1;					/* offset is smashed.  */
#endif

  return acc;
}

const char *
ctf_strerror (int err)
{
  return (const char *) (strerror (err));
}

/* Set the CTF library client version to the specified version.  If version is
   zero, we just return the default library version number.  */
int
ctf_version (int version)
{
  if (version < 0)
    {
      errno = EINVAL;
      return -1;
    }

  if (version > 0)
    {
      /*  Dynamic version switching is not presently supported. */
      if (version != CTF_VERSION)
	{
	  errno = ENOTSUP;
	  return -1;
	}
      ctf_dprintf ("ctf_version: client using version %d\n", version);
      _libctf_version = version;
    }

  return _libctf_version;
}

void
libctf_init_debug (void)
{
  static int inited;
  if (!inited)
    {
      _libctf_debug = getenv ("LIBCTF_DEBUG") != NULL;
      inited = 1;
    }
}

void ctf_setdebug (int debug)
{
  /* Ensure that libctf_init_debug() has been called, so that we don't get our
     debugging-on-or-off smashed by the next call.  */

  libctf_init_debug();
  _libctf_debug = debug;
  ctf_dprintf ("CTF debugging set to %i\n", debug);
}

int ctf_getdebug (void)
{
  return _libctf_debug;
}

_libctf_printflike_ (1, 2)
void ctf_dprintf (const char *format, ...)
{
  if (_libctf_debug)
    {
      va_list alist;

      va_start (alist, format);
      fflush (stdout);
      (void) fputs ("libctf DEBUG: ", stderr);
      (void) vfprintf (stderr, format, alist);
      va_end (alist);
    }
}
