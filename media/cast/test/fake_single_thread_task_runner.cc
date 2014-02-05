// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/fake_single_thread_task_runner.h"

#include "base/logging.h"
#include "base/time/tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {
namespace test {

FakeSingleThreadTaskRunner::FakeSingleThreadTaskRunner(
    base::SimpleTestTickClock* clock)
    : clock_(clock) {}

FakeSingleThreadTaskRunner::~FakeSingleThreadTaskRunner() {}

bool FakeSingleThreadTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  EXPECT_GE(delay, base::TimeDelta());
  PostedTask posed_task(from_here,
                        task,
                        clock_->NowTicks(),
                        delay,
                        base::TestPendingTask::NESTABLE);

  tasks_.insert(std::make_pair(posed_task.GetTimeToRun(), posed_task));
  return false;
}

bool FakeSingleThreadTaskRunner::RunsTasksOnCurrentThread() const {
  return true;
}

void FakeSingleThreadTaskRunner::RunTasks() {
  while(true) {
    // Run all tasks equal or older than current time.
    std::multimap<base::TimeTicks, PostedTask>::iterator it = tasks_.begin();
    if (it == tasks_.end())
      return;  // No more tasks.

    PostedTask task = it->second;
    if (clock_->NowTicks() < task.GetTimeToRun())
      return;

    tasks_.erase(it);
    task.task.Run();
  }
}

bool FakeSingleThreadTaskRunner::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace test
}  // namespace cast
}  // namespace media
