// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/wall_clock_time_source.h"

#include "base/logging.h"

namespace media {

WallClockTimeSource::WallClockTimeSource()
    : tick_clock_(&default_tick_clock_), ticking_(false), playback_rate_(1.0) {
}

WallClockTimeSource::~WallClockTimeSource() {
}

void WallClockTimeSource::StartTicking() {
  DVLOG(1) << __FUNCTION__;
  base::AutoLock auto_lock(lock_);
  DCHECK(!ticking_);
  ticking_ = true;
  reference_wall_ticks_ = tick_clock_->NowTicks();
}

void WallClockTimeSource::StopTicking() {
  DVLOG(1) << __FUNCTION__;
  base::AutoLock auto_lock(lock_);
  DCHECK(ticking_);
  base_time_ = CurrentMediaTime_Locked();
  ticking_ = false;
  reference_wall_ticks_ = tick_clock_->NowTicks();
}

void WallClockTimeSource::SetPlaybackRate(double playback_rate) {
  DVLOG(1) << __FUNCTION__ << "(" << playback_rate << ")";
  base::AutoLock auto_lock(lock_);
  // Estimate current media time using old rate to use as a new base time for
  // the new rate.
  if (ticking_) {
    base_time_ = CurrentMediaTime_Locked();
    reference_wall_ticks_ = tick_clock_->NowTicks();
  }

  playback_rate_ = playback_rate;
}

void WallClockTimeSource::SetMediaTime(base::TimeDelta time) {
  DVLOG(1) << __FUNCTION__ << "(" << time.InMicroseconds() << ")";
  base::AutoLock auto_lock(lock_);
  CHECK(!ticking_);
  base_time_ = time;
}

base::TimeDelta WallClockTimeSource::CurrentMediaTime() {
  base::AutoLock auto_lock(lock_);
  return CurrentMediaTime_Locked();
}

bool WallClockTimeSource::GetWallClockTimes(
    const std::vector<base::TimeDelta>& media_timestamps,
    std::vector<base::TimeTicks>* wall_clock_times) {
  base::AutoLock auto_lock(lock_);
  if (!ticking_ || !playback_rate_)
    return false;

  DCHECK(wall_clock_times->empty());
  wall_clock_times->reserve(media_timestamps.size());
  for (const auto& media_timestamp : media_timestamps) {
    wall_clock_times->push_back(
        reference_wall_ticks_ +
        base::TimeDelta::FromMicroseconds(
            (media_timestamp - base_time_).InMicroseconds() / playback_rate_));
  }
  return true;
}

base::TimeDelta WallClockTimeSource::CurrentMediaTime_Locked() {
  lock_.AssertAcquired();
  if (!ticking_ || !playback_rate_)
    return base_time_;

  base::TimeTicks now = tick_clock_->NowTicks();
  return base_time_ +
         base::TimeDelta::FromMicroseconds(
             (now - reference_wall_ticks_).InMicroseconds() * playback_rate_);
}

}  // namespace media
