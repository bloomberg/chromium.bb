// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_SWAP_METRICS_OBSERVER_H_
#define CONTENT_BROWSER_MEMORY_SWAP_METRICS_OBSERVER_H_

#include "base/time/time.h"
#include "base/timer/timer.h"

namespace content {

// This class observes system's swapping behavior to collect metrics.
// Metrics can be platform-specific.
class SwapMetricsObserver {
 public:
  // This returns nullptr when swap metrics are not available on the system.
  static SwapMetricsObserver* GetInstance();

  // Starts observing swap metrics.
  void Start();

  // Stop observing swap metrics.
  void Stop();

 protected:
  SwapMetricsObserver();
  virtual ~SwapMetricsObserver();

  // Periodically called to update swap metrics.
  void UpdateMetrics();

  // Platform-dependent parts of UpdateMetrics(). |interval| is the elapsed time
  // since the last UpdateMetrics() call. |interval| will be zero when this
  // function is called for the first time.
  virtual void UpdateMetricsInternal(base::TimeDelta interval) = 0;

 private:
  // The interval between metrics updates.
  base::TimeDelta update_interval_;

  // A periodic timer to update swap metrics.
  base::RepeatingTimer timer_;

  // Holds the last TimeTicks when swap metrics are updated.
  base::TimeTicks last_ticks_;

  DISALLOW_COPY_AND_ASSIGN(SwapMetricsObserver);
};

}  // namespace

#endif  // CONTENT_BROWSER_MEMORY_SWAP_METRICS_OBSERVER_H_
