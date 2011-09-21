// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DELAYED_WORK_SCHEDULER_H_
#define CHROME_BROWSER_POLICY_DELAYED_WORK_SCHEDULER_H_
#pragma once

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/timer.h"

namespace policy {

// A mockable class for scheduling and cancelling a delayed task.
// This is only a thin wrapper around base::OneShotTimer.
// This class is not thread-safe: all its methods should be called on the same
// thread, and the callback will happen on that same thread.
class DelayedWorkScheduler {
 public:
  DelayedWorkScheduler();
  virtual ~DelayedWorkScheduler();

  // Cancels the delayed work task.
  virtual void CancelDelayedWork();

  // Posts a new delayed task.
  virtual void PostDelayedWork(const base::Closure& callback, int64 delay);

 private:
  base::OneShotTimer<DelayedWorkScheduler> timer_;
  base::Closure callback_;

  void DoDelayedWork();

  DISALLOW_COPY_AND_ASSIGN(DelayedWorkScheduler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DELAYED_WORK_SCHEDULER_H_
