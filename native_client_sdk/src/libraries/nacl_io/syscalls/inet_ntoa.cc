// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/ossocket.h"
#if defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__)

#include <string.h>
#include "sdk_util/macros.h"

static uint8_t GetByte(const void* addr, int byte) {
  const char* buf = static_cast<const char*>(addr);
  return static_cast<uint8_t>(buf[byte]);
}

EXTERN_C_BEGIN

char* inet_ntoa(struct in_addr in) {
  static char addr[INET_ADDRSTRLEN];
  snprintf(addr, INET_ADDRSTRLEN, "%u.%u.%u.%u",
           GetByte(&in, 0), GetByte(&in, 1),
           GetByte(&in, 2), GetByte(&in, 3));
  return addr;
}

EXTERN_C_END

#endif  // defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__)
