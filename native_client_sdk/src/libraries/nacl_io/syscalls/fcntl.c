/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <stdarg.h>

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

int fcntl(int fd, int cmd, ...) {
  va_list ap;
  va_start(ap, cmd);
  char* arg = va_arg(ap, char*);
  va_end(ap);
  return ki_fcntl(fd, cmd, arg);
}
