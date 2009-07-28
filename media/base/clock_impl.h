// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementation of Clock based on the system clock.  ClockImpl uses linear
// interpolation to calculate the current media time since the last time
// SetTime() was called.
//
// ClockImpl is not thread-safe and must be externally locked.

#ifndef MEDIA_BASE_CLOCK_IMPL_H_
#define MEDIA_BASE_CLOCK_IMPL_H_

#include "media/base/clock.h"

namespace media {

// Type for a static function pointer that acts as a time source.
typedef base::Time(TimeProvider)();

class ClockImpl : public Clock {
 public:
  ClockImpl(TimeProvider* time_provider);
  virtual ~ClockImpl();

  // Clock implementation.
  virtual base::TimeDelta Play();
  virtual base::TimeDelta Pause();
  virtual void SetPlaybackRate(float playback_rate);
  virtual void SetTime(const base::TimeDelta& time);
  virtual base::TimeDelta Elapsed() const;

 private:
  // Returns the current media time treating the given time as the latest
  // value as returned by |time_provider_|.
  base::TimeDelta ElapsedViaProvidedTime(const base::Time& time) const;

  // Function returning current time in base::Time units.
  TimeProvider* time_provider_;

  // Whether the clock is running.
  bool playing_;

  // The system clock time when this clock last starting playing or had its
  // time set via SetTime().
  base::Time reference_;

  // Current accumulated amount of media time.  The remaining portion must be
  // calculated by comparing the system time to the reference time.
  base::TimeDelta media_time_;

  // Current playback rate.
  float playback_rate_;

  DISALLOW_COPY_AND_ASSIGN(ClockImpl);
};

}  // namespace media

#endif  // MEDIA_BASE_CLOCK_IMPL_H_
