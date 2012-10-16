// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(__native_client__)
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#endif

extern "C" {

#if defined(__native_client__)

char* getcwd(char* buf, size_t size) __attribute__ ((weak));
int unlink(const char* pathname) __attribute__ ((weak));
int mkdir(const char* pathname, mode_t mode) __attribute__ ((weak));

char* getcwd(char* buf, size_t size) {
  if (size < 2) {
    errno = ERANGE;
    return NULL;
  }

  return strncpy(buf, ".", size);
}

int unlink(const char* pathname) {
  errno = ENOSYS;
  return -1;
}

int mkdir(const char* pathname, mode_t mode) {
  errno = ENOSYS;
  return -1;
}

#endif

}  // extern "C"
