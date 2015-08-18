// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_clock_device_default.h"

#include "media/base/buffers.h"

namespace chromecast {
namespace media {
namespace {

// Return true if transition from |state1| to |state2| is a valid state
// transition.
inline static bool IsValidStateTransition(MediaClockDevice::State state1,
                                          MediaClockDevice::State state2) {
  if (state2 == state1)
    return true;

  // All states can transition to |kStateError|.
  if (state2 == MediaClockDevice::kStateError)
    return true;

  // All the other valid FSM transitions.
  switch (state1) {
    case MediaClockDevice::kStateUninitialized:
      return state2 == MediaClockDevice::kStateIdle;
    case MediaClockDevice::kStateIdle:
      return state2 == MediaClockDevice::kStateRunning ||
             state2 == MediaClockDevice::kStateUninitialized;
    case MediaClockDevice::kStateRunning:
      return state2 == MediaClockDevice::kStateIdle;
    case MediaClockDevice::kStateError:
      return state2 == MediaClockDevice::kStateUninitialized;
    default:
      return false;
  }
}

}  // namespace

MediaClockDeviceDefault::MediaClockDeviceDefault()
    : state_(kStateUninitialized),
      media_time_(::media::kNoTimestamp()),
      rate_(0.0f) {
  thread_checker_.DetachFromThread();
}

MediaClockDeviceDefault::~MediaClockDeviceDefault() {
}

MediaClockDevice::State MediaClockDeviceDefault::GetState() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return state_;
}

bool MediaClockDeviceDefault::SetState(State new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsValidStateTransition(state_, new_state));

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

bool MediaClockDeviceDefault::ResetTimeline(int64_t time_microseconds) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, kStateIdle);
  media_time_ = base::TimeDelta::FromMicroseconds(time_microseconds);
  return true;
}

bool MediaClockDeviceDefault::SetRate(float rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ == kStateRunning) {
    base::TimeTicks now = base::TimeTicks::Now();
    media_time_ = media_time_ + (now - stc_) * rate_;
    stc_ = now;
  }

  rate_ = rate;
  return true;
}

int64_t MediaClockDeviceDefault::GetTimeMicroseconds() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ != kStateRunning)
    return media_time_.InMicroseconds();

  if (media_time_ == ::media::kNoTimestamp())
    return media_time_.InMicroseconds();

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta interpolated_media_time =
      media_time_ + (now - stc_) * rate_;
  return interpolated_media_time.InMicroseconds();
}

}  // namespace media
}  // namespace chromecast
