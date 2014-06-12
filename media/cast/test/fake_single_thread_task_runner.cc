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
    : clock_(clock),
      fail_on_next_task_(false) {}

FakeSingleThreadTaskRunner::~FakeSingleThreadTaskRunner() {}

bool FakeSingleThreadTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  if (fail_on_next_task_) {
    LOG(FATAL) << "Infinite task-add loop detected.";
  }
  CHECK(delay >= base::TimeDelta());
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
  while (true) {
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

void FakeSingleThreadTaskRunner::Sleep(base::TimeDelta t) {
  base::TimeTicks run_until = clock_->NowTicks() + t;
  while (1) {
    // If we run more than 100000 iterations, we've probably
    // hit some sort of case where a new task is posted every
    // time that we invoke a task, and we can't make progress
    // anymore. If that happens, set fail_on_next_task_ to true
    // and throw an error when the next task is posted.
    for (int i = 0; i < 100000; i++) {
      // Run all tasks equal or older than current time.
      std::multimap<base::TimeTicks, PostedTask>::iterator it = tasks_.begin();
      if (it == tasks_.end()) {
        clock_->Advance(run_until - clock_->NowTicks());
        return;
      }

      PostedTask task = it->second;
      if (run_until < task.GetTimeToRun()) {
        clock_->Advance(run_until - clock_->NowTicks());
        return;
      }

      clock_->Advance(task.GetTimeToRun() - clock_->NowTicks());
      tasks_.erase(it);
      task.task.Run();
    }
    // Instead of failing immediately, we fail when the next task is
    // added so that the backtrace will include the task that was added.
    fail_on_next_task_ = true;
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
