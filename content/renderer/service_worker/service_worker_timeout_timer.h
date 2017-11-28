// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_TIMEOUT_TIMER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_TIMEOUT_TIMER_H_

#include <map>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"

namespace base {

class TickClock;

}  // namespace base

namespace content {

// ServiceWorkerTimeoutTimer manages two types of timeouts: the long standing
// event timeout and the idle timeout.
//
// S13nServiceWorker:
// 1) Event timeout: when an event starts, StartEvent() records the expiration
// time of the event (kEventTimeout). If EndEvent() has not been called within
// the timeout time, |abort_callback| passed to StartEvent() is called.
// 2) Idle timeout: when a certain time has passed (kIdleDelay) since all of
// events have ended, ServiceWorkerTimeoutTimer calls the |idle_callback|.
// |idle_callback| will be continuously called at a certain interval
// (kUpdateInterval) until the next event starts.
//
// The lifetime of ServiceWorkerTimeoutTimer is the same with the worker
// thread. If ServiceWorkerTimeoutTimer is destructed while there are inflight
// events, all |abort_callback|s will be immediately called.
//
// Non-S13nServiceWorker:
// Does nothing except calls the abort callbacks upon destruction.
class CONTENT_EXPORT ServiceWorkerTimeoutTimer {
 public:
  explicit ServiceWorkerTimeoutTimer(base::RepeatingClosure idle_callback);
  // For testing.
  ServiceWorkerTimeoutTimer(base::RepeatingClosure idle_callback,
                            std::unique_ptr<base::TickClock> tick_clock);
  ~ServiceWorkerTimeoutTimer();

  // StartEvent() should be called at the beginning of an event. It returns an
  // event id. The event id should be passed to EndEvent() when the event has
  // finished.
  // See the class comment to know when |abort_callback| runs.
  int StartEvent(base::OnceCallback<void(int /* event_id */)> abort_callback);
  void EndEvent(int event_id);

  // Idle timeout duration since the last event has finished.
  static constexpr base::TimeDelta kIdleDelay =
      base::TimeDelta::FromSeconds(30);
  // Duration of the long standing event timeout since StartEvent() has been
  // called.
  static constexpr base::TimeDelta kEventTimeout =
      base::TimeDelta::FromMinutes(5);
  // ServiceWorkerTimeoutTimer periodically updates the timeout state by
  // kUpdateInterval.
  static constexpr base::TimeDelta kUpdateInterval =
      base::TimeDelta::FromSeconds(30);

 private:
  // Updates the internal states and fires timeout callbacks if any.
  void UpdateStatus();

  // For event timeouts. Contains only inflight events.
  std::map<int /* event_id */, base::OnceClosure> abort_callbacks_;

  // For long standing event timeouts. Contains both inflight and settled
  // events. If an |event_id| does not exist in |abort_callbacks_|, the event
  // already finished successfully.
  base::queue<std::pair<base::TimeTicks, int /* event_id */>>
      event_timeout_times_;

  // For idle timeouts. The time the service worker started being considered
  // idle. This time is null if there are any inflight events.
  base::TimeTicks idle_time_;

  // For idle timeouts. Invoked when UpdateStatus() is called after
  // |idle_time_|.
  base::RepeatingClosure idle_callback_;

  // |timer_| invokes UpdateEventStatus() periodically.
  base::RepeatingTimer timer_;

  std::unique_ptr<base::TickClock> tick_clock_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_TIMEOUT_TIMER_H_
