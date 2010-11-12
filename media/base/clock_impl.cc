// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/base/buffers.h"
#include "media/base/clock_impl.h"

namespace media {

ClockImpl::ClockImpl(TimeProvider* time_provider)
    : time_provider_(time_provider),
      playing_(false),
      playback_rate_(1.0f) {
}

ClockImpl::~ClockImpl() {
}

base::TimeDelta ClockImpl::Play() {
  DCHECK(!playing_);
  reference_ = time_provider_();
  playing_ = true;
  return media_time_;
}

base::TimeDelta ClockImpl::Pause() {
  DCHECK(playing_);
  // Save our new accumulated amount of media time.
  media_time_ = Elapsed();
  playing_ = false;
  return media_time_;
}

void ClockImpl::SetTime(const base::TimeDelta& time) {
  if (time == kNoTimestamp) {
    NOTREACHED();
    return;
  }
  if (playing_) {
    reference_ = time_provider_();
  }
  media_time_ = time;
}

void ClockImpl::SetPlaybackRate(float playback_rate) {
  if (playing_) {
    base::Time time = time_provider_();
    media_time_ = ElapsedViaProvidedTime(time);
    reference_ = time;
  }
  playback_rate_ = playback_rate;
}

base::TimeDelta ClockImpl::Elapsed() const {
  if (!playing_) {
    return media_time_;
  }
  return ElapsedViaProvidedTime(time_provider_());
}

base::TimeDelta ClockImpl::ElapsedViaProvidedTime(
    const base::Time& time) const {
  // TODO(scherkus): floating point badness scaling time by playback rate.
  int64 now_us = (time - reference_).InMicroseconds();
  now_us = static_cast<int64>(now_us * playback_rate_);
  return media_time_ + base::TimeDelta::FromMicroseconds(now_us);
}

}  // namespace media
