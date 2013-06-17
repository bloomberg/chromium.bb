// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/vsync_time_source.h"

#include "base/logging.h"

namespace cc {

scoped_refptr<VSyncTimeSource> VSyncTimeSource::Create(
    VSyncProvider* vsync_provider, NotificationDisableOption option) {
  return make_scoped_refptr(new VSyncTimeSource(vsync_provider, option));
}

VSyncTimeSource::VSyncTimeSource(
    VSyncProvider* vsync_provider, NotificationDisableOption option)
    : active_(false),
      notification_requested_(false),
      vsync_provider_(vsync_provider),
      client_(NULL),
      disable_option_(option) {}

VSyncTimeSource::~VSyncTimeSource() {}

void VSyncTimeSource::SetClient(TimeSourceClient* client) {
  client_ = client;
}

void VSyncTimeSource::SetActive(bool active) {
  if (active_ == active)
    return;
  active_ = active;
  if (active_ && !notification_requested_) {
    notification_requested_ = true;
    vsync_provider_->RequestVSyncNotification(this);
  }
  if (!active_ && disable_option_ == DISABLE_SYNCHRONOUSLY) {
    notification_requested_ = false;
    vsync_provider_->RequestVSyncNotification(NULL);
  }
}

bool VSyncTimeSource::Active() const {
  return active_;
}

base::TimeTicks VSyncTimeSource::LastTickTime() {
  return last_tick_time_;
}

base::TimeTicks VSyncTimeSource::NextTickTime() {
  return Active() ? last_tick_time_ + interval_ : base::TimeTicks();
}

void VSyncTimeSource::SetTimebaseAndInterval(base::TimeTicks,
                                             base::TimeDelta interval) {
  interval_ = interval;
}

void VSyncTimeSource::DidVSync(base::TimeTicks frame_time) {
  last_tick_time_ = frame_time;
  if (disable_option_ == DISABLE_SYNCHRONOUSLY) {
    DCHECK(active_);
  } else if (!active_) {
    if (notification_requested_) {
      notification_requested_ = false;
      vsync_provider_->RequestVSyncNotification(NULL);
    }
    return;
  }
  if (client_)
    client_->OnTimerTick();
}

}  // namespace cc
