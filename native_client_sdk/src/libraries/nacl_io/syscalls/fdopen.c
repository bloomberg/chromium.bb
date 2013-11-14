/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__native_client__) && defined(__GLIBC__)

static ssize_t fdopen_read(void *cookie, char *buf, size_t size) {
  return read((int) cookie, buf, size);
}

static ssize_t fdopen_write(void *cookie, const char *buf, size_t size) {
  return write((int) cookie, buf, size);
}

static int fdopen_seek(void *cookie, off_t *offset, int whence) {
  off_t ret = lseek((int) cookie, *offset, whence);
  if (ret < 0) {
    return -1;
  }
  *offset = ret;
  return 0;
}

static int fdopen_close(void *cookie) {
  return close((int) cookie);
}

static cookie_io_functions_t fdopen_functions = {
  fdopen_read,
  fdopen_write,
  fdopen_seek,
  fdopen_close,
};

/*
 * TODO(bradnelson): Remove this glibc is fixed.
 * See: https://code.google.com/p/nativeclient/issues/detail?id=3362
 * Workaround fdopen in glibc failing due to it vetting the provided file
 * handles with fcntl which is not currently interceptable.
 */
FILE *fdopen(int fildes, const char *mode) {
  return fopencookie((void *) fildes, mode, fdopen_functions);
}

#endif  /* defined(__native_client__) && defined(__GLIBC__) */
