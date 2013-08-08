// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/ossocket.h"

#if defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__)

static __thread int __h_errno__;

extern "C" int *__h_errno_location() {
  return &__h_errno__;
}

#endif  // defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__)
