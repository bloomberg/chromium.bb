// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_MOCK_TIME_TASK_RUNNER_H_
#define BASE_TEST_TEST_MOCK_TIME_TASK_RUNNER_H_

#include <queue>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/test/test_pending_task.h"
#include "base/threading/thread_checker.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace base {

// Runs pending tasks in the order of the tasks' post time + delay, and keeps
// track of a mock (virtual) tick clock time that can be fast-forwarded.
//
// TestMockTimeTaskRunner has the following properties:
//
//   - Methods RunsTasksOnCurrentThread() and Post[Delayed]Task() can be called
//     from any thread, but the rest of the methods must be called on the same
//     thread the TaskRunner was created on.
//   - It allows for reentrancy, in that it handles the running of tasks that in
//     turn call back into it (e.g., to post more tasks).
//   - Tasks are stored in a priority queue, and executed in the increasing
//     order of post time + delay.
//   - Non-nestable tasks are not supported.
//   - Tasks aren't guaranteed to be destroyed immediately after they're run.
//
// This is a slightly more sophisticated version of TestSimpleTaskRunner, in
// that it supports running delayed tasks in the correct temporal order.
class TestMockTimeTaskRunner : public base::SingleThreadTaskRunner {
 public:
  TestMockTimeTaskRunner();

  // Fast-forwards virtual time by |delta|, causing all tasks with a remaining
  // delay less than or equal to |delta| to be executed.
  void FastForwardBy(base::TimeDelta delta);

  // Fast-forwards virtual time just until all tasks are executed.
  void FastForwardUntilNoTasksRemain();

  // Executes all tasks that have no remaining delay. Tasks with a remaining
  // delay greater than zero will remain enqueued, and no virtual time will
  // elapse.
  void RunUntilIdle();

  // Returns the current virtual time.
  TimeTicks GetCurrentMockTime() const;

  // Returns a TickClock that uses the mock time of |this| as its time source.
  scoped_ptr<TickClock> GetMockTickClock() const;

  bool HasPendingTask() const;
  TimeDelta NextPendingTaskDelay() const;

  // SingleThreadTaskRunner:
  bool RunsTasksOnCurrentThread() const override;
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               TimeDelta delay) override;
  bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      TimeDelta delay) override;

 protected:
  ~TestMockTimeTaskRunner() override;

  // Called before the next task to run is selected, so that subclasses have a
  // last chance to make sure all tasks are posted.
  virtual void OnBeforeSelectingTask();

  // Called after the current mock time has been incremented so that subclasses
  // can react to the passing of time.
  virtual void OnAfterTimePassed();

  // Called after each task is run so that subclasses may perform additional
  // activities, e.g., pump additional task runners.
  virtual void OnAfterTaskRun();

 private:
  // Predicate that defines a strict weak temporal ordering of tasks.
  class TemporalOrder {
   public:
    bool operator()(const TestPendingTask& first_task,
                    const TestPendingTask& second_task) const;
  };

  typedef std::priority_queue<TestPendingTask,
                              std::vector<TestPendingTask>,
                              TemporalOrder> TaskPriorityQueue;

  // Returns the |next_task| to run if there is any with a running time that is
  // at most |reference| + |max_delta|. This additional complexity is required
  // so that |max_delta| == TimeDelta::Max() can be supported.
  bool DequeueNextTask(const base::TimeTicks& reference,
                       const base::TimeDelta& max_delta,
                       TestPendingTask* next_task);

  base::ThreadChecker thread_checker_;
  base::TimeTicks now_;

  // Temporally ordered heap of pending tasks. Must only be accessed while the
  // |tasks_lock_| is held.
  TaskPriorityQueue tasks_;
  base::Lock tasks_lock_;

  DISALLOW_COPY_AND_ASSIGN(TestMockTimeTaskRunner);
};

}  // namespace base

#endif  // BASE_TEST_TEST_MOCK_TIME_TASK_RUNNER_H_
