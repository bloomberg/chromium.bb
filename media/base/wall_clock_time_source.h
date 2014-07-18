// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WALL_CLOCK_TIME_SOURCE_H_
#define MEDIA_BASE_WALL_CLOCK_TIME_SOURCE_H_

#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/base/time_source.h"

namespace base {
class TickClock;
}

namespace media {

// A time source that uses interpolation based on the system clock.
class MEDIA_EXPORT WallClockTimeSource : public TimeSource {
 public:
  WallClockTimeSource();
  virtual ~WallClockTimeSource();

  // TimeSource implementation.
  virtual void StartTicking() OVERRIDE;
  virtual void StopTicking() OVERRIDE;
  virtual void SetPlaybackRate(float playback_rate) OVERRIDE;
  virtual void SetMediaTime(base::TimeDelta time) OVERRIDE;
  virtual base::TimeDelta CurrentMediaTime() OVERRIDE;

  void SetTickClockForTesting(scoped_ptr<base::TickClock> tick_clock);

 private:
  scoped_ptr<base::TickClock> tick_clock_;
  bool ticking_;

  // While ticking we can interpolate the current media time by measuring the
  // delta between our reference ticks and the current system ticks and scaling
  // that time by the playback rate.
  float playback_rate_;
  base::TimeDelta base_time_;
  base::TimeTicks reference_wall_ticks_;

  DISALLOW_COPY_AND_ASSIGN(WallClockTimeSource);
};

}  // namespace media

#endif  // MEDIA_BASE_WALL_CLOCK_TIME_SOURCE_H_
