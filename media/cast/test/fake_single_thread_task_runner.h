// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_FAKE_TASK_RUNNER_H_
#define MEDIA_CAST_TEST_FAKE_TASK_RUNNER_H_

#include <map>

#include "base/basictypes.h"
#include "base/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_pending_task.h"

namespace media {
namespace cast {
namespace test {

typedef base::TestPendingTask PostedTask;

class FakeSingleThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit FakeSingleThreadTaskRunner(base::SimpleTestTickClock* clock);

  void RunTasks();

  // Note: Advances |clock_|.
  void Sleep(base::TimeDelta t);

  // base::SingleThreadTaskRunner implementation.
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;

  bool RunsTasksOnCurrentThread() const override;

  // This function is currently not used, and will return false.
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override;

 protected:
  ~FakeSingleThreadTaskRunner() override;

 private:
  base::SimpleTestTickClock* const clock_;
  std::multimap<base::TimeTicks, PostedTask> tasks_;
  bool fail_on_next_task_;

  DISALLOW_COPY_AND_ASSIGN(FakeSingleThreadTaskRunner);
};

}  // namespace test
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_FAKE_TASK_RUNNER_H_
