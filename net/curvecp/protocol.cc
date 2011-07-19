// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/curvecp/protocol.h"

#include "base/basictypes.h"

namespace net {

void uint16_pack(uchar* dest, uint16 val) {
  *dest++ = val;
  val >>= 8;
  *dest++ = val;
  val >>= 8;
}

uint16 uint16_unpack(uchar* src) {
  uint16 result;
  result = src[1];
  result <<= 8;
  result |= src[0];
  return result;
}

void uint32_pack(uchar* dest, uint32 val) {
  *dest++ = val;
  val >>= 8;
  *dest++ = val;
  val >>= 8;
  *dest++ = val;
  val >>= 8;
  *dest++ = val;
  val >>= 8;
}

uint32 uint32_unpack(uchar* src) {
  uint32 result;
  result = src[3];
  result <<= 8;
  result |= src[2];
  result <<= 8;
  result |= src[1];
  result <<= 8;
  result |= src[0];
  return result;
}

void uint64_pack(uchar* dest, uint64 val) {
  *dest++ = val;
  val >>= 8;
  *dest++ = val;
  val >>= 8;
  *dest++ = val;
  val >>= 8;
  *dest++ = val;
  val >>= 8;
  *dest++ = val;
  val >>= 8;
  *dest++ = val;
  val >>= 8;
  *dest++ = val;
  val >>= 8;
  *dest++ = val;
  val >>= 8;
}

uint64 uint64_unpack(uchar* src) {
  uint64 result;
  result = src[7];
  result <<= 8;
  result |= src[6];
  result <<= 8;
  result |= src[5];
  result <<= 8;
  result |= src[4];
  result <<= 8;
  result |= src[3];
  result <<= 8;
  result |= src[2];
  result <<= 8;
  result |= src[1];
  result <<= 8;
  result |= src[0];
  return result;
}

}  // namespace net
