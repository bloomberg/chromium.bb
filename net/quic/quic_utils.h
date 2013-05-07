// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Some helpers for quic

#ifndef NET_QUIC_QUIC_UTILS_H_
#define NET_QUIC_QUIC_UTILS_H_

#include "net/base/int128.h"
#include "net/base/net_export.h"
#include "net/quic/quic_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE QuicUtils {
 public:
  // returns the 64 bit FNV1a hash of the data.  See
  // http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param
  static uint64 FNV1a_64_Hash(const char* data, int len);

  // returns the 128 bit FNV1a hash of the data.  See
  // http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param
  static uint128 FNV1a_128_Hash(const char* data, int len);

  // SerializeUint128 writes |v| in little-endian form to |out|.
  static void SerializeUint128(uint128 v, uint8* out);

  // ParseUint128 parses a little-endian uint128 from |in| and returns it.
  static uint128 ParseUint128(const uint8* in);

  // Returns the name of the QuicRstStreamErrorCode as a char*
  static const char* StreamErrorToString(QuicRstStreamErrorCode error);

  // Returns the name of the QuicErrorCode as a char*
  static const char* ErrorToString(QuicErrorCode error);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_UTILS_H_
