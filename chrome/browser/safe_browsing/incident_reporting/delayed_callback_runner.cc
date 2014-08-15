// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/delayed_callback_runner.h"

#include "base/location.h"

namespace safe_browsing {

DelayedCallbackRunner::DelayedCallbackRunner(
    base::TimeDelta delay,
    const scoped_refptr<base::TaskRunner>& task_runner)
    : task_runner_(task_runner),
      next_callback_(callbacks_.end()),
      timer_(FROM_HERE, delay, this, &DelayedCallbackRunner::OnTimer) {
}

DelayedCallbackRunner::~DelayedCallbackRunner() {
}

void DelayedCallbackRunner::RegisterCallback(const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  callbacks_.push_back(callback);
}

void DelayedCallbackRunner::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Nothing to do if the runner is already running or nothing has been added.
  if (next_callback_ != callbacks_.end() || callbacks_.empty())
    return;

  // Prime the system with the first callback.
  next_callback_ = callbacks_.begin();

  // Point the starter pistol in the air and pull the trigger.
  timer_.Reset();
}

void DelayedCallbackRunner::OnTimer() {
  // Run the next callback on the task runner.
  task_runner_->PostTask(FROM_HERE, *next_callback_);

  // Remove this callback and get ready for the next if there is one.
  next_callback_ = callbacks_.erase(next_callback_);
  if (next_callback_ != callbacks_.end())
    timer_.Reset();
}

}  // namespace safe_browsing
