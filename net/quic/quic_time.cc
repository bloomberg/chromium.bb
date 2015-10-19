// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_time.h"

#include <stdint.h>

#include "base/logging.h"

namespace net {

uint64 QuicWallTime::ToUNIXSeconds() const {
  return seconds_;
}

bool QuicWallTime::IsAfter(QuicWallTime other) const {
  return seconds_ > other.seconds_;
}

bool QuicWallTime::IsBefore(QuicWallTime other) const {
  return seconds_ < other.seconds_;
}

bool QuicWallTime::IsZero() const {
  return seconds_ == 0;
}

QuicTime::Delta QuicWallTime::AbsoluteDifference(QuicWallTime other) const {
  uint64 d;

  if (seconds_ > other.seconds_) {
    d = seconds_ - other.seconds_;
  } else {
    d = other.seconds_ - seconds_;
  }

  if (d > static_cast<uint64>(kint64max)) {
    d = kint64max;
  }
  return QuicTime::Delta::FromSeconds(d);
}

QuicWallTime QuicWallTime::Add(QuicTime::Delta delta) const {
  uint64 seconds = seconds_ + delta.ToSeconds();
  if (seconds < seconds_) {
    seconds = kuint64max;
  }
  return QuicWallTime(seconds);
}

// TODO(ianswett) Test this.
QuicWallTime QuicWallTime::Subtract(QuicTime::Delta delta) const {
  uint64 seconds = seconds_ - delta.ToSeconds();
  if (seconds > seconds_) {
    seconds = 0;
  }
  return QuicWallTime(seconds);
}

}  // namespace net
