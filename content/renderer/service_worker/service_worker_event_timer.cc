// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_event_timer.h"

#include "base/time/time.h"

namespace content {

// static
constexpr base::TimeDelta ServiceWorkerEventTimer::kIdleDelay;

ServiceWorkerEventTimer::ServiceWorkerEventTimer(
    base::RepeatingClosure idle_callback)
    : idle_callback_(std::move(idle_callback)) {
  // |idle_callback_| will be invoked if no event happens in |kIdleDelay|.
  idle_timer_.Start(FROM_HERE, kIdleDelay, idle_callback_);
}

ServiceWorkerEventTimer::~ServiceWorkerEventTimer() = default;

void ServiceWorkerEventTimer::StartEvent() {
  idle_timer_.Stop();
  ++num_inflight_events_;
}

void ServiceWorkerEventTimer::EndEvent() {
  DCHECK(!idle_timer_.IsRunning());
  DCHECK_GT(num_inflight_events_, 0u);
  --num_inflight_events_;
  if (num_inflight_events_ == 0) {
    idle_timer_.Start(FROM_HERE, kIdleDelay, idle_callback_);
  }
}

}  // namespace content
