// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_MONOTONIC_CLOCK_H_
#define CHROMECAST_MEDIA_BASE_MONOTONIC_CLOCK_H_

#include <stdint.h>

namespace chromecast {
namespace media {

// Returns the monotonic time (from CLOCK_MONOTONIC) in microseconds.
// Why not use base::TimeTicks? Because we explicitly need time from
// CLOCK_MONOTONIC to match the timestamps from CMA GetRenderingDelay(), and
// base::TimeTicks has no guarantees about which clock it uses (and it could
// change whenever they feel like it upstream).
int64_t MonotonicClockNow();

// Interface that provides the monotonic time.
class MonotonicClock {
 public:
  virtual ~MonotonicClock() = default;
  // Returns the monotonic time in microseconds.
  virtual int64_t Now() = 0;
};

// Default implementation of MonotonicClock that uses MonotonicClockNow().
class DefaultMonotonicClock : public MonotonicClock {
 public:
  DefaultMonotonicClock();
  ~DefaultMonotonicClock() override;

  // MonotonicClock implementation:
  int64_t Now() override;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_MONOTONIC_CLOCK_H_
