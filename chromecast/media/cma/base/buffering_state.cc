// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/buffering_state.h"

#include <sstream>

#include "base/logging.h"
#include "media/base/buffers.h"

namespace chromecast {
namespace media {

BufferingConfig::BufferingConfig(
    base::TimeDelta low_level_threshold,
    base::TimeDelta high_level_threshold)
    : low_level_threshold_(low_level_threshold),
      high_level_threshold_(high_level_threshold) {
}

BufferingConfig::~BufferingConfig() {
}


BufferingState::BufferingState(
    const scoped_refptr<BufferingConfig>& config,
    const base::Closure& state_changed_cb,
    const HighLevelBufferCB& high_level_buffer_cb)
    : config_(config),
      state_changed_cb_(state_changed_cb),
      high_level_buffer_cb_(high_level_buffer_cb),
      state_(kLowLevel),
      media_time_(::media::kNoTimestamp()),
      max_rendering_time_(::media::kNoTimestamp()),
      buffered_time_(::media::kNoTimestamp()) {
}

BufferingState::~BufferingState() {
}

void BufferingState::OnConfigChanged() {
  state_ = GetBufferLevelState();
}

void BufferingState::SetMediaTime(base::TimeDelta media_time) {
  media_time_ = media_time;
  switch (state_) {
    case kLowLevel:
    case kMediumLevel:
    case kHighLevel:
      UpdateState(GetBufferLevelState());
      break;
    case kEosReached:
      break;
  }
}

void BufferingState::SetMaxRenderingTime(base::TimeDelta max_rendering_time) {
  max_rendering_time_ = max_rendering_time;
}

base::TimeDelta BufferingState::GetMaxRenderingTime() const {
  return max_rendering_time_;
}

void BufferingState::SetBufferedTime(base::TimeDelta buffered_time) {
  buffered_time_ = buffered_time;
  switch (state_) {
    case kLowLevel:
    case kMediumLevel:
    case kHighLevel:
      UpdateState(GetBufferLevelState());
      break;
    case kEosReached:
      break;
  }
}

void BufferingState::NotifyEos() {
  UpdateState(kEosReached);
}

void BufferingState::NotifyMaxCapacity(base::TimeDelta buffered_time) {
  if (media_time_ == ::media::kNoTimestamp() ||
      buffered_time == ::media::kNoTimestamp()) {
    LOG(WARNING) << "Max capacity with no timestamp";
    return;
  }
  base::TimeDelta buffer_duration = buffered_time - media_time_;
  if (buffer_duration < config_->high_level())
    high_level_buffer_cb_.Run(buffer_duration);
}

std::string BufferingState::ToString() const {
  std::ostringstream s;
  s << "state=" << state_
    << " media_time_ms=" << media_time_.InMilliseconds()
    << " buffered_time_ms=" << buffered_time_.InMilliseconds()
    << " low_level_ms=" << config_->low_level().InMilliseconds()
    << " high_level_ms=" << config_->high_level().InMilliseconds();
  return s.str();
}

BufferingState::State BufferingState::GetBufferLevelState() const {
  if (media_time_ == ::media::kNoTimestamp() ||
      buffered_time_ == ::media::kNoTimestamp()) {
    return kLowLevel;
  }

  base::TimeDelta buffer_duration = buffered_time_ - media_time_;
  if (buffer_duration < config_->low_level())
    return kLowLevel;
  if (buffer_duration >= config_->high_level())
    return kHighLevel;
  return kMediumLevel;
}

void BufferingState::UpdateState(State new_state) {
  if (new_state == state_)
    return;

  state_ = new_state;
  if (!state_changed_cb_.is_null())
    state_changed_cb_.Run();
}

}  // namespace media
}  // namespace chromecast
