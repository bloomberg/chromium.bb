/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <limits.h>
#include <string.h>

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

char* getcwd(char* buf, size_t size) {
  // If size is 0, allocate as much as we need.
  if (size == 0) {
    char stack_buf[PATH_MAX + 1];
    if (!ki_getcwd(stack_buf, PATH_MAX))
      return NULL;
    size = strlen(stack_buf) + 1;
  }
  // Allocate the buffer if needed
  if (buf == NULL) {
    buf = (char*)malloc(size);
  }
  return ki_getcwd(buf, size);
}
