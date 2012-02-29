// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_runner_test_template.h"

namespace base {

TaskTracker::TaskTracker() {}

TaskTracker::~TaskTracker() {}

void TaskTracker::RunTask(int i) {
  AutoLock lock(task_run_counts_lock_);
  ++task_run_counts_[i];
}

std::map<int, int> TaskTracker::GetTaskRunCounts() const {
  AutoLock lock(task_run_counts_lock_);
  return task_run_counts_;
}

}  // namespace base
