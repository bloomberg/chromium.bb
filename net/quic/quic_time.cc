// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_time.h"

#include "base/logging.h"

namespace net {

// Highest number of microseconds that DateTimeOffset can hold.
const int64 kQuicInfiniteTimeUs = GG_INT64_C(0x7fffffffffffffff) / 10;

QuicTime::Delta::Delta(base::TimeDelta delta)
    : delta_(delta) {
}

// static
QuicTime::Delta QuicTime::Delta::Zero() {
  return QuicTime::Delta::FromMicroseconds(0);
}

// static
QuicTime::Delta QuicTime::Delta::Infinite() {
  return QuicTime::Delta::FromMicroseconds(kQuicInfiniteTimeUs);
}

// static
QuicTime::Delta QuicTime::Delta::FromSeconds(int64 seconds) {
  return QuicTime::Delta(base::TimeDelta::FromSeconds(seconds));
}

// static
QuicTime::Delta QuicTime::Delta::FromMilliseconds(int64 ms) {
  return QuicTime::Delta(base::TimeDelta::FromMilliseconds(ms));
}

// static
QuicTime::Delta QuicTime::Delta::FromMicroseconds(int64 us) {
  return QuicTime::Delta(base::TimeDelta::FromMicroseconds(us));
}

int64 QuicTime::Delta::ToSeconds() const {
  return delta_.InSeconds();
}

int64 QuicTime::Delta::ToMilliseconds() const {
  return delta_.InMilliseconds();
}

int64 QuicTime::Delta::ToMicroseconds() const {
  return delta_.InMicroseconds();
}

QuicTime::Delta QuicTime::Delta::Add(const Delta& delta) const {
  return QuicTime::Delta::FromMicroseconds(ToMicroseconds() +
                                           delta.ToMicroseconds());
}

QuicTime::Delta QuicTime::Delta::Subtract(const Delta& delta) const {
  return QuicTime::Delta::FromMicroseconds(ToMicroseconds() -
                                           delta.ToMicroseconds());
}

bool QuicTime::Delta::IsZero() const {
  return delta_.InMicroseconds() == 0;
}

bool QuicTime::Delta::IsInfinite() const {
  return delta_.InMicroseconds() == kQuicInfiniteTimeUs;
}

// static
QuicTime QuicTime::Zero() {
  return QuicTime(base::TimeTicks());
}

QuicTime::QuicTime(base::TimeTicks ticks)
    : ticks_(ticks) {
}

int64 QuicTime::ToDebuggingValue() const {
  return (ticks_ - base::TimeTicks()).InMicroseconds();
}

bool QuicTime::IsInitialized() const {
  return ticks_ != base::TimeTicks();
}

QuicTime QuicTime::Add(const Delta& delta) const {
  return QuicTime(ticks_ + delta.delta_);
}

QuicTime QuicTime::Subtract(const Delta& delta) const {
  return QuicTime(ticks_ - delta.delta_);
}

QuicTime::Delta QuicTime::Subtract(const QuicTime& other) const {
  return QuicTime::Delta(ticks_ - other.ticks_);
}

}  // namespace net
