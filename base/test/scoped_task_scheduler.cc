// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_task_scheduler.h"

#include "base/task_scheduler/task_scheduler.h"

namespace base {
namespace test {

ScopedTaskScheduler::ScopedTaskScheduler() {
  DCHECK(!TaskScheduler::GetInstance());

  // Create a TaskScheduler with a single thread to make tests deterministic.
  constexpr int kMaxThreads = 1;
  TaskScheduler::CreateAndSetSimpleTaskScheduler(kMaxThreads);
  task_scheduler_ = TaskScheduler::GetInstance();
}

ScopedTaskScheduler::~ScopedTaskScheduler() {
  DCHECK_EQ(task_scheduler_, TaskScheduler::GetInstance());
  TaskScheduler::GetInstance()->Shutdown();
  TaskScheduler::GetInstance()->JoinForTesting();
  TaskScheduler::SetInstance(nullptr);
}

}  // namespace test
}  // namespace base
