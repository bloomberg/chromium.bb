// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/delayed_work_scheduler.h"

namespace policy {

DelayedWorkScheduler::DelayedWorkScheduler() {
}

DelayedWorkScheduler::~DelayedWorkScheduler() {
  timer_.Stop();
}

void DelayedWorkScheduler::DoDelayedWork() {
  callback_.Run();
}

void DelayedWorkScheduler::CancelDelayedWork() {
  timer_.Stop();
}

void DelayedWorkScheduler::PostDelayedWork(
    const base::Closure& callback,
    int64 delay) {
  callback_ = callback;
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(delay),
               this,
               &DelayedWorkScheduler::DoDelayedWork);
}

}  // namespace policy
