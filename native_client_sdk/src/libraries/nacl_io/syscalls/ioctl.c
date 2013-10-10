/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <stdarg.h>

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"
#include "nacl_io/dbgprint.h"

int ioctl(int fd, unsigned long request, ...) {
  va_list ap;
  va_start(ap, request);
  int rtn = ki_ioctl(fd, request, ap);
  va_end(ap);
  return rtn;
}
