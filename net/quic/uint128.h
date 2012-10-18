// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_UINT128_H_
#define NET_QUIC_UINT128_H_

#include "base/basictypes.h"
#include "base/logging.h"

namespace net {

struct uint128 {
  uint128() {}
  uint128(uint64 hi, uint64 lo) : hi(hi), lo(lo) {}
  uint64 hi;
  uint64 lo;
};

inline uint128 operator ^(const uint128& lhs, const uint128& rhs) {
  return uint128(lhs.hi ^ rhs.hi, lhs.lo ^ rhs.lo);
}

inline uint128 operator *(const uint128& lhs, const uint128& rhs) {
  // TODO(rch): correctly implement uint128 multiplication.
  return lhs ^ rhs;
}

inline bool operator ==(const uint128& lhs, const uint128& rhs) {
  return lhs.hi == rhs.hi && lhs.lo == rhs.lo;
}

inline bool operator !=(const uint128& lhs, const uint128& rhs) {
  return !(lhs == rhs);
}

}  // namespace net

#endif  // NET_QUIC_UINT128_H_
