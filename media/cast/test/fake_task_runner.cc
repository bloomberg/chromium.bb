// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/fake_task_runner.h"

#include "base/time/tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {
namespace test {

FakeTaskRunner::FakeTaskRunner(base::SimpleTestTickClock* clock)
    : clock_(clock) {
}

FakeTaskRunner::~FakeTaskRunner() {}

bool FakeTaskRunner::PostDelayedTask(const tracked_objects::Location& from_here,
                                     const base::Closure& task,
                                     base::TimeDelta delay) {
  EXPECT_GE(delay, base::TimeDelta());
  PostedTask posed_task(from_here, task, clock_->NowTicks(), delay,
                        base::TestPendingTask::NESTABLE);

  tasks_.insert(std::make_pair(posed_task.GetTimeToRun(), posed_task));
  return false;
}

bool FakeTaskRunner::RunsTasksOnCurrentThread() const {
  return true;
}

void FakeTaskRunner::RunTasks() {
  for (;;) {
    // Run all tasks equal or older than current time.
    std::multimap<base::TimeTicks, PostedTask>::iterator it = tasks_.begin();
    if (it == tasks_.end()) return;  // No more tasks.

    PostedTask task = it->second;
    if (clock_->NowTicks() < task.GetTimeToRun()) return;

    tasks_.erase(it);
    task.task.Run();
  }
}

}  // namespace test
}  // namespace cast
}  // namespace media
