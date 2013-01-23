// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_bandwidth.h"

#include "base/logging.h"
#include "base/time.h"

namespace net {

// Highest number that QuicBandwidth can hold.
const int64 kQuicInfiniteBandwidth = 0x7fffffffffffffffll;

// static
QuicBandwidth QuicBandwidth::Zero() {
  return QuicBandwidth(0);
}

// static
QuicBandwidth QuicBandwidth::FromBitsPerSecond(int64 bits_per_second) {
  return QuicBandwidth(bits_per_second);
}

// static
QuicBandwidth QuicBandwidth::FromKBitsPerSecond(int64 k_bits_per_second) {
  DCHECK(k_bits_per_second < kQuicInfiniteBandwidth / 1000);
  return QuicBandwidth(k_bits_per_second * 1000);
}

// static
QuicBandwidth QuicBandwidth::FromBytesPerSecond(int64 bytes_per_second) {
  DCHECK(bytes_per_second < kQuicInfiniteBandwidth / 8);
  return QuicBandwidth(bytes_per_second * 8);
}

// static
QuicBandwidth QuicBandwidth::FromKBytesPerSecond(int64 k_bytes_per_second) {
  DCHECK(k_bytes_per_second < kQuicInfiniteBandwidth / 8000);
  return QuicBandwidth(k_bytes_per_second * 8000);
}

// static
QuicBandwidth QuicBandwidth::FromBytesAndTimeDelta(int64 bytes,
                                                   QuicTime::Delta delta) {
  DCHECK_LT(bytes,
            kQuicInfiniteBandwidth / (8 * base::Time::kMicrosecondsPerSecond));
  int64 bytes_per_second = (bytes * base::Time::kMicrosecondsPerSecond) /
      delta.ToMicroseconds();
  return QuicBandwidth(bytes_per_second * 8);
}

QuicBandwidth::QuicBandwidth(int64 bits_per_second)
    : bits_per_second_(bits_per_second) {
  DCHECK_GE(bits_per_second, 0);
}

int64 QuicBandwidth::ToBitsPerSecond() const {
  return bits_per_second_;
}

int64 QuicBandwidth::ToKBitsPerSecond() const {
  return bits_per_second_ / 1000;
}

int64 QuicBandwidth::ToBytesPerSecond() const {
  return bits_per_second_ / 8;
}

int64 QuicBandwidth::ToKBytesPerSecond() const {
  return bits_per_second_ / 8000;
}

bool QuicBandwidth::IsZero() const {
  return (bits_per_second_ == 0);
}

QuicBandwidth QuicBandwidth::Add(const QuicBandwidth& delta) const {
  return QuicBandwidth(bits_per_second_ + delta.bits_per_second_);
}

QuicBandwidth QuicBandwidth::Subtract(const QuicBandwidth& delta) const {
  return QuicBandwidth(bits_per_second_ - delta.bits_per_second_);
}

}  // namespace net
