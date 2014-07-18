// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/wall_clock_time_source.h"

#include "base/logging.h"
#include "base/time/default_tick_clock.h"

namespace media {

WallClockTimeSource::WallClockTimeSource()
    : tick_clock_(new base::DefaultTickClock()),
      ticking_(false),
      playback_rate_(1.0f) {
}

WallClockTimeSource::~WallClockTimeSource() {
}

void WallClockTimeSource::StartTicking() {
  DCHECK(!ticking_);
  ticking_ = true;
  reference_wall_ticks_ = tick_clock_->NowTicks();
}

void WallClockTimeSource::StopTicking() {
  DCHECK(ticking_);
  base_time_ = CurrentMediaTime();
  ticking_ = false;
  reference_wall_ticks_ = tick_clock_->NowTicks();
}

void WallClockTimeSource::SetPlaybackRate(float playback_rate) {
  // Estimate current media time using old rate to use as a new base time for
  // the new rate.
  if (ticking_) {
    base_time_ = CurrentMediaTime();
    reference_wall_ticks_ = tick_clock_->NowTicks();
  }

  playback_rate_ = playback_rate;
}

void WallClockTimeSource::SetMediaTime(base::TimeDelta time) {
  CHECK(!ticking_);
  base_time_ = time;
}

base::TimeDelta WallClockTimeSource::CurrentMediaTime() {
  if (!ticking_)
    return base_time_;

  base::TimeTicks now = tick_clock_->NowTicks();
  return base_time_ +
         base::TimeDelta::FromMicroseconds(
             (now - reference_wall_ticks_).InMicroseconds() * playback_rate_);
}

void WallClockTimeSource::SetTickClockForTesting(
    scoped_ptr<base::TickClock> tick_clock) {
  tick_clock_.swap(tick_clock);
}

}  // namespace media
