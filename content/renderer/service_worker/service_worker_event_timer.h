// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_EVENT_TIMER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_EVENT_TIMER_H_

#include "base/callback.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"

namespace content {

// Manages idle timeout of events.
// TODO(crbug.com/774374) Implement long running timer too.
class CONTENT_EXPORT ServiceWorkerEventTimer {
 public:
  // |idle_callback| will be called when a certain period has passed since the
  // last event ended.
  explicit ServiceWorkerEventTimer(base::RepeatingClosure idle_callback);
  ~ServiceWorkerEventTimer();

  void StartEvent();
  void EndEvent();

  // Idle timeout duration since the last event has finished.
  static constexpr base::TimeDelta kIdleDelay =
      base::TimeDelta::FromSeconds(30);

 private:
  uint64_t num_inflight_events_ = 0;

  // This is repeating timer since |idle_timer_| will fire repeatedly once it's
  // considered as idle.
  base::RepeatingTimer idle_timer_;
  base::RepeatingClosure idle_callback_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_EVENT_TIMER_H_
