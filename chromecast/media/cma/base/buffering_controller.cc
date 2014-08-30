// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/buffering_controller.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chromecast/media/cma/base/buffering_state.h"
#include "chromecast/media/cma/base/cma_logging.h"
#include "media/base/buffers.h"

namespace chromecast {
namespace media {

BufferingController::BufferingController(
    const scoped_refptr<BufferingConfig>& config,
    const BufferingNotificationCB& buffering_notification_cb)
    : config_(config),
      buffering_notification_cb_(buffering_notification_cb),
      is_buffering_(false),
      begin_buffering_time_(base::Time()),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
}

BufferingController::~BufferingController() {
  // Some weak pointers might possibly be invalidated here.
  DCHECK(thread_checker_.CalledOnValidThread());
}

void BufferingController::UpdateHighLevelThreshold(
    base::TimeDelta high_level_threshold) {
  // Can only decrease the high level threshold.
  if (high_level_threshold > config_->high_level())
    return;
  CMALOG(kLogControl) << "High buffer threshold: "
                      << high_level_threshold.InMilliseconds();
  config_->set_high_level(high_level_threshold);

  // Make sure the low level threshold is somewhat consistent.
  // Currently, we set it to one third of the high level threshold:
  // this value could be adjusted in the future.
  base::TimeDelta low_level_threshold = high_level_threshold / 3;
  if (low_level_threshold <= config_->low_level()) {
    CMALOG(kLogControl) << "Low buffer threshold: "
                        << low_level_threshold.InMilliseconds();
    config_->set_low_level(low_level_threshold);
  }

  // Signal all the streams the config has changed.
  for (StreamList::iterator it = stream_list_.begin();
       it != stream_list_.end(); ++it) {
    (*it)->OnConfigChanged();
  }

  // Once all the streams have been notified, the buffering state must be
  // updated (no notification is received from the streams).
  OnBufferingStateChanged(false, false);
}

scoped_refptr<BufferingState> BufferingController::AddStream() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Add a new stream to the list of streams being monitored.
  scoped_refptr<BufferingState> buffering_state(new BufferingState(
      config_,
      base::Bind(&BufferingController::OnBufferingStateChanged, weak_this_,
                 false, false),
      base::Bind(&BufferingController::UpdateHighLevelThreshold, weak_this_)));
  stream_list_.push_back(buffering_state);

  // Update the state and force a notification to the streams.
  // TODO(damienv): Should this be a PostTask ?
  OnBufferingStateChanged(true, false);

  return buffering_state;
}

void BufferingController::SetMediaTime(base::TimeDelta time) {
  for (StreamList::iterator it = stream_list_.begin();
       it != stream_list_.end(); ++it) {
    (*it)->SetMediaTime(time);
  }
}

base::TimeDelta BufferingController::GetMaxRenderingTime() const {
  base::TimeDelta max_rendering_time(::media::kNoTimestamp());
  for (StreamList::const_iterator it = stream_list_.begin();
       it != stream_list_.end(); ++it) {
    base::TimeDelta max_stream_rendering_time =
        (*it)->GetMaxRenderingTime();
    if (max_stream_rendering_time == ::media::kNoTimestamp())
      return ::media::kNoTimestamp();
    if (max_rendering_time == ::media::kNoTimestamp() ||
        max_stream_rendering_time < max_rendering_time) {
      max_rendering_time = max_stream_rendering_time;
    }
  }
  return max_rendering_time;
}

void BufferingController::Reset() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_buffering_ = false;
  stream_list_.clear();
}

void BufferingController::OnBufferingStateChanged(
    bool force_notification, bool buffering_timeout) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Log the state of each stream.
  DumpState();

  bool is_low_buffering = IsLowBufferLevel();
  bool is_high_buffering = !is_low_buffering;
  if (!buffering_timeout) {
    // Hysteresis:
    // - to leave buffering, not only should we leave the low buffer level state
    //   but we should go to the high buffer level state (medium is not enough).
    is_high_buffering = IsHighBufferLevel();
  }

  bool is_buffering_prv = is_buffering_;
  if (is_buffering_) {
    if (is_high_buffering)
      is_buffering_ = false;
  } else {
    if (is_low_buffering)
      is_buffering_ = true;
  }

  // Start buffering.
  if (is_buffering_ && !is_buffering_prv) {
    begin_buffering_time_ = base::Time::Now();
  }

  // End buffering.
  if (is_buffering_prv && !is_buffering_) {
    // TODO(damienv): |buffering_user_time| could be a UMA histogram.
    base::Time current_time = base::Time::Now();
    base::TimeDelta buffering_user_time = current_time - begin_buffering_time_;
    CMALOG(kLogControl)
        << "Buffering took: "
        << buffering_user_time.InMilliseconds() << "ms";
  }

  if (is_buffering_prv != is_buffering_ || force_notification)
    buffering_notification_cb_.Run(is_buffering_);
}

bool BufferingController::IsHighBufferLevel() {
  if (stream_list_.empty())
    return true;

  bool is_high_buffering = true;
  for (StreamList::iterator it = stream_list_.begin();
       it != stream_list_.end(); ++it) {
    BufferingState::State stream_state = (*it)->GetState();
    is_high_buffering = is_high_buffering &&
        ((stream_state == BufferingState::kHighLevel) ||
         (stream_state == BufferingState::kEosReached));
  }
  return is_high_buffering;
}

bool BufferingController::IsLowBufferLevel() {
  if (stream_list_.empty())
    return false;

  for (StreamList::iterator it = stream_list_.begin();
       it != stream_list_.end(); ++it) {
    BufferingState::State stream_state = (*it)->GetState();
    if (stream_state == BufferingState::kLowLevel)
      return true;
  }

  return false;
}

void BufferingController::DumpState() const {
  CMALOG(kLogControl) << __FUNCTION__;
  for (StreamList::const_iterator it = stream_list_.begin();
       it != stream_list_.end(); ++it) {
    CMALOG(kLogControl) << (*it)->ToString();
  }
}

}  // namespace media
}  // namespace chromecast
