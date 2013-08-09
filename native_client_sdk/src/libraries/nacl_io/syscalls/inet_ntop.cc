// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/ossocket.h"
#if defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__)

#include <errno.h>
#include <string.h>

#include <iostream>
#include <sstream>
#include <string>

#include "sdk_util/macros.h"


EXTERN_C_BEGIN

const char* inet_ntop(int af, const void* src, char* dst, socklen_t size) {
  if (AF_INET == af) {
    if (size < INET_ADDRSTRLEN) {
      errno = ENOSPC;
      return NULL;
    }
    struct in_addr in;
    memcpy(&in, src, sizeof(in));
    char* result = inet_ntoa(in);
    memcpy(dst, result, strlen(result) + 1);
    return dst;
  }

  if (AF_INET6 == af) {
    if (size < INET6_ADDRSTRLEN) {
      errno = ENOSPC;
      return NULL;
    }
    const uint8_t* tuples = static_cast<const uint8_t*>(src);
    std::stringstream output;
    for (int i = 0; i < 8; i++) {
      uint16_t tuple = (tuples[2*i] << 8) + tuples[2*i+1];
      output << std::hex << tuple;
      if (i < 7) {
        output << ":";
      }
    }
    memcpy(dst, output.str().c_str(), output.str().size() + 1);
    return dst;
  }

  errno = EAFNOSUPPORT;
  return NULL;
}

EXTERN_C_END

#endif  // defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__)

