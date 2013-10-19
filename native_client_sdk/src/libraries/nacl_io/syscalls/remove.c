/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

#if defined(__GLIBC__) && defined(__native_client__)
// Glibc's remove(3) and unlink(2) entry points are not yet hooked up
// to the lower level IRT interfaces.  Therefore the only way to intercept
// these calls is to override them here.
// TODO(sbc): remove this once glibc plumbing is in place for remove/unlink

int remove(const char* path) {
  return ki_remove(path);
}
#endif
