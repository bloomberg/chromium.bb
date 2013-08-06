// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/ossocket.h"

#ifdef PROVIDES_SOCKET_API

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <sstream>
#include <string>

static inline uint8_t get_byte(const void* addr, int byte) {
  const char* buf = static_cast<const char*>(addr);
  return static_cast<uint8_t>(buf[byte]);
}

char* inet_ntoa(struct in_addr in) {
  static char addr[INET_ADDRSTRLEN];
  snprintf(addr, INET_ADDRSTRLEN, "%u.%u.%u.%u",
           get_byte(&in, 0), get_byte(&in, 1),
           get_byte(&in, 2), get_byte(&in, 3));
  return addr;
}

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

#endif // PROVIDES_SOCKET_API
