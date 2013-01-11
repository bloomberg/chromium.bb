// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/vsync_time_source.h"

namespace cc {

scoped_refptr<VSyncTimeSource> VSyncTimeSource::create(
    VSyncProvider* vsync_provider) {
  return make_scoped_refptr(new VSyncTimeSource(vsync_provider));
}

VSyncTimeSource::VSyncTimeSource(VSyncProvider* vsync_provider)
  : vsync_provider_(vsync_provider)
  , client_(0)
  , active_(false)
  , notification_requested_(false) {}

VSyncTimeSource::~VSyncTimeSource() {}

void VSyncTimeSource::setClient(TimeSourceClient* client) {
  client_ = client;
}

void VSyncTimeSource::setActive(bool active) {
  if (active_ == active)
    return;
  active_ = active;
  // The notification will be lazily disabled in the callback to ensure
  // we get notified of the frame immediately following a quick on-off-on
  // transition.
  if (active_ && !notification_requested_) {
    notification_requested_ = true;
    vsync_provider_->RequestVSyncNotification(this);
  }
}

bool VSyncTimeSource::active() const {
  return active_;
}

base::TimeTicks VSyncTimeSource::lastTickTime() {
  return last_tick_time_;
}

base::TimeTicks VSyncTimeSource::nextTickTime() {
  return active() ? last_tick_time_ + interval_ : base::TimeTicks();
}

void VSyncTimeSource::setTimebaseAndInterval(base::TimeTicks,
                                             base::TimeDelta interval) {
  interval_ = interval;
}

void VSyncTimeSource::DidVSync(base::TimeTicks frame_time) {
  last_tick_time_ = frame_time;
  if (!active_) {
    if (notification_requested_) {
      notification_requested_ = false;
      vsync_provider_->RequestVSyncNotification(NULL);
    }
    return;
  }
  if (client_)
    client_->onTimerTick();
}

}  // namespace cc
