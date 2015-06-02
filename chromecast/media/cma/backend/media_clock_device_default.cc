// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_clock_device_default.h"

#include "media/base/buffers.h"

namespace chromecast {
namespace media {

MediaClockDeviceDefault::MediaClockDeviceDefault()
    : state_(kStateUninitialized),
      media_time_(::media::kNoTimestamp()) {
  DetachFromThread();
}

MediaClockDeviceDefault::~MediaClockDeviceDefault() {
}

MediaClockDevice::State MediaClockDeviceDefault::GetState() const {
  DCHECK(CalledOnValidThread());
  return state_;
}

bool MediaClockDeviceDefault::SetState(State new_state) {
  DCHECK(CalledOnValidThread());
  if (!MediaClockDevice::IsValidStateTransition(state_, new_state))
    return false;

  if (new_state == state_)
    return true;

  state_ = new_state;

  if (state_ == kStateRunning) {
    stc_ = base::TimeTicks::Now();
    DCHECK(media_time_ != ::media::kNoTimestamp());
    return true;
  }

  if (state_ == kStateIdle) {
    media_time_ = ::media::kNoTimestamp();
    return true;
  }

  return true;
}

bool MediaClockDeviceDefault::ResetTimeline(base::TimeDelta time) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state_, kStateIdle);
  media_time_ = time;
  return true;
}

bool MediaClockDeviceDefault::SetRate(float rate) {
  DCHECK(CalledOnValidThread());
  if (state_ == kStateRunning) {
    base::TimeTicks now = base::TimeTicks::Now();
    media_time_ = media_time_ + (now - stc_) * rate_;
    stc_ = now;
  }

  rate_ = rate;
  return true;
}

base::TimeDelta MediaClockDeviceDefault::GetTime() {
  DCHECK(CalledOnValidThread());
  if (state_ != kStateRunning)
    return media_time_;

  if (media_time_ == ::media::kNoTimestamp())
    return ::media::kNoTimestamp();

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta interpolated_media_time =
      media_time_ + (now - stc_) * rate_;
  return interpolated_media_time;
}

}  // namespace media
}  // namespace chromecast
