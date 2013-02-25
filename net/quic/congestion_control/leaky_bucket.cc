// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/leaky_bucket.h"

#include "base/time.h"

namespace net {

LeakyBucket::LeakyBucket(QuicBandwidth draining_rate)
    : bytes_(0),
      time_last_updated_(QuicTime::Zero()),
      draining_rate_(draining_rate) {
}

void LeakyBucket::SetDrainingRate(QuicTime now, QuicBandwidth draining_rate) {
  Update(now);
  draining_rate_ = draining_rate;
}

void LeakyBucket::Add(QuicTime now, QuicByteCount bytes) {
  Update(now);
  bytes_ += bytes;
}

QuicTime::Delta LeakyBucket::TimeRemaining(QuicTime now) {
  Update(now);
  return QuicTime::Delta::FromMicroseconds(
      (bytes_ * base::Time::kMicrosecondsPerSecond) /
      draining_rate_.ToBytesPerSecond());
}

QuicByteCount LeakyBucket::BytesPending(QuicTime now) {
  Update(now);
  return bytes_;
}

void LeakyBucket::Update(QuicTime now) {
  QuicTime::Delta elapsed_time = now.Subtract(time_last_updated_);
  QuicByteCount bytes_cleared = draining_rate_.ToBytesPerPeriod(elapsed_time);
  if (bytes_cleared >= bytes_) {
    bytes_ = 0;
  } else {
    bytes_ -= bytes_cleared;
  }
  time_last_updated_ = now;
}

}  // namespace net
