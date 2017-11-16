// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "gin/v8_background_task_runner.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/task_scheduler/task_traits.h"

namespace gin {

namespace {

constexpr base::TaskTraits kBackgroundThreadTaskTraits = {
    base::TaskPriority::USER_VISIBLE};

}  // namespace

void V8BackgroundTaskRunner::PostTask(std::unique_ptr<v8::Task> task) {
  base::PostTaskWithTraits(FROM_HERE, kBackgroundThreadTaskTraits,
                           base::BindOnce(&v8::Task::Run, std::move(task)));
}

void V8BackgroundTaskRunner::PostDelayedTask(std::unique_ptr<v8::Task> task,
                                             double delay_in_seconds) {
  base::PostDelayedTaskWithTraits(
      FROM_HERE, kBackgroundThreadTaskTraits,
      base::BindOnce(&v8::Task::Run, std::move(task)),
      base::TimeDelta::FromSecondsD(delay_in_seconds));
}

void V8BackgroundTaskRunner::PostIdleTask(std::unique_ptr<v8::IdleTask> task) {
  NOTREACHED() << "Idle tasks are not supported on background threads.";
}

bool V8BackgroundTaskRunner::IdleTasksEnabled() {
  // No idle tasks on background threads.
  return false;
}

// static
size_t V8BackgroundTaskRunner::NumberOfAvailableBackgroundThreads() {
  return std::max(1, base::TaskScheduler::GetInstance()
                         ->GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                             kBackgroundThreadTaskTraits));
}

}  // namespace gin
