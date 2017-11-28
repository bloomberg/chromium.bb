// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_timeout_timer.h"

#include "base/stl_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "content/common/service_worker/service_worker_utils.h"

namespace content {

namespace {

int NextEventId() {
  // Event id should not start from zero since HashMap in Blink requires
  // non-zero keys.
  static int s_next_event_id = 1;
  CHECK_LT(s_next_event_id, std::numeric_limits<int>::max());
  return s_next_event_id++;
}

}  // namespace

// static
constexpr base::TimeDelta ServiceWorkerTimeoutTimer::kIdleDelay;
constexpr base::TimeDelta ServiceWorkerTimeoutTimer::kEventTimeout;
constexpr base::TimeDelta ServiceWorkerTimeoutTimer::kUpdateInterval;

ServiceWorkerTimeoutTimer::ServiceWorkerTimeoutTimer(
    base::RepeatingClosure idle_callback)
    : ServiceWorkerTimeoutTimer(std::move(idle_callback),
                                std::make_unique<base::DefaultTickClock>()) {}

ServiceWorkerTimeoutTimer::ServiceWorkerTimeoutTimer(
    base::RepeatingClosure idle_callback,
    std::unique_ptr<base::TickClock> tick_clock)
    : idle_callback_(std::move(idle_callback)),
      tick_clock_(std::move(tick_clock)) {
  // |idle_callback_| will be invoked if no event happens in |kIdleDelay|.
  idle_time_ = tick_clock_->NowTicks() + kIdleDelay;
  timer_.Start(FROM_HERE, kUpdateInterval,
               base::BindRepeating(&ServiceWorkerTimeoutTimer::UpdateStatus,
                                   base::Unretained(this)));
}

ServiceWorkerTimeoutTimer::~ServiceWorkerTimeoutTimer() {
  // Abort all callbacks.
  for (auto& it : abort_callbacks_)
    std::move(it.second).Run();
};

int ServiceWorkerTimeoutTimer::StartEvent(
    base::OnceCallback<void(int /* event_id */)> abort_callback) {
  idle_time_ = base::TimeTicks();
  const int event_id = NextEventId();

  DCHECK(!base::ContainsKey(abort_callbacks_, event_id));
  abort_callbacks_[event_id] =
      base::BindOnce(std::move(abort_callback), event_id);
  event_timeout_times_.emplace(tick_clock_->NowTicks() + kEventTimeout,
                               event_id);
  return event_id;
}

void ServiceWorkerTimeoutTimer::EndEvent(int event_id) {
  DCHECK(base::ContainsKey(abort_callbacks_, event_id));
  abort_callbacks_.erase(event_id);
  if (abort_callbacks_.empty())
    idle_time_ = tick_clock_->NowTicks() + kIdleDelay;
}

void ServiceWorkerTimeoutTimer::UpdateStatus() {
  if (!ServiceWorkerUtils::IsServicificationEnabled())
    return;

  base::TimeTicks now = tick_clock_->NowTicks();

  // Abort all events exceeding |kEventTimeout|.
  while (!event_timeout_times_.empty()) {
    const auto& p = event_timeout_times_.front();
    if (p.first > now)
      break;
    int event_id = p.second;
    event_timeout_times_.pop();
    // If |event_id| is not found in |abort_callbacks_|, it means the event has
    // successfully finished.
    auto iter = abort_callbacks_.find(event_id);
    if (iter == abort_callbacks_.end())
      continue;
    base::OnceClosure callback = std::move(iter->second);
    abort_callbacks_.erase(iter);
    std::move(callback).Run();
  }

  // If |abort_callbacks_| is empty, the worker is now idle.
  if (abort_callbacks_.empty() && idle_time_.is_null())
    idle_time_ = tick_clock_->NowTicks() + kIdleDelay;

  if (!idle_time_.is_null() && idle_time_ < now)
    idle_callback_.Run();
}

}  // namespace content
