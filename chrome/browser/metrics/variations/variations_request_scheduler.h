// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_REQUEST_SCHEDULER_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_REQUEST_SCHEDULER_H_

#include "base/bind.h"
#include "base/timer.h"

namespace chrome_variations {

// A helper class that makes VariationsService requests at the correct times.
class VariationsRequestScheduler {
 public:
  // |task| is the closure to call when the scheduler deems ready.
  explicit VariationsRequestScheduler(const base::Closure& task);
  virtual ~VariationsRequestScheduler();

  // Resets the scheduler if it is currently on a timer.
  void Reset();

 private:
  // The task scheduled by this class.
  base::Closure task_;

  // The timer used to repeatedly ping the server. Keep this as an instance
  // member so if VariationsRequestScheduler goes out of scope, the timer is
  // automatically canceled.
  base::RepeatingTimer<VariationsRequestScheduler> timer_;
};

}  // namespace chrome_variations

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_REQUEST_SCHEDULER_H_
