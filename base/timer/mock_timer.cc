// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/mock_timer.h"

#include "base/test/test_simple_task_runner.h"

namespace base {

MockTimer::MockTimer(bool retain_user_task, bool is_repeating)
    : Timer(retain_user_task, is_repeating, &clock_),
      test_task_runner_(MakeRefCounted<TestSimpleTaskRunner>()) {
  Timer::SetTaskRunner(test_task_runner_);
}

MockTimer::~MockTimer() = default;

void MockTimer::SetTaskRunner(scoped_refptr<SequencedTaskRunner> task_runner) {
  NOTREACHED() << "MockTimer doesn't support SetTaskRunner().";
}

void MockTimer::Fire() {
  DCHECK(IsRunning());
  clock_.Advance(std::max(TimeDelta(), desired_run_time() - clock_.NowTicks()));

  // Do not use TestSimpleTaskRunner::RunPendingTasks() here. As RunPendingTasks
  // overrides ThreadTaskRunnerHandle when it runs tasks, tasks posted by timer
  // tasks to TTRH go to |test_task_runner_|, though they should be posted to
  // the original task runner.
  for (TestPendingTask& task : test_task_runner_->TakePendingTasks())
    std::move(task.task).Run();
}

}  // namespace base
