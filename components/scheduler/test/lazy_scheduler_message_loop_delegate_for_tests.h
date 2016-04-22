// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_TEST_LAZY_SCHEDULER_MESSAGE_LOOP_DELEGATE_FOR_TESTS_H_
#define COMPONENTS_SCHEDULER_TEST_LAZY_SCHEDULER_MESSAGE_LOOP_DELEGATE_FOR_TESTS_H_

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/time/tick_clock.h"
#include "components/scheduler/child/scheduler_tqm_delegate.h"

namespace scheduler {

// This class connects the scheduler to a MessageLoop, but unlike
// SchedulerMessageLoopDelegate it allows the message loop to be created lazily
// after the scheduler has been brought up. This is needed in testing scenarios
// where Blink is initialized before a MessageLoop has been created.
//
// TODO(skyostil): Fix the relevant test suites and remove this class
// (crbug.com/495659).
class LazySchedulerMessageLoopDelegateForTests : public SchedulerTqmDelegate {
 public:
  static scoped_refptr<LazySchedulerMessageLoopDelegateForTests> Create();

  // SchedulerTqmDelegate implementation
  void SetDefaultTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void RestoreDefaultTaskRunner() override;
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override;
  bool RunsTasksOnCurrentThread() const override;
  bool IsNested() const override;
  base::TimeTicks NowTicks() override;

 private:
  LazySchedulerMessageLoopDelegateForTests();
  ~LazySchedulerMessageLoopDelegateForTests() override;

  bool HasMessageLoop() const;
  base::MessageLoop* EnsureMessageLoop() const;

  mutable base::MessageLoop* message_loop_;
  base::PlatformThreadId thread_id_;

  // A task runner which hasn't yet been overridden in the message loop.
  mutable scoped_refptr<base::SingleThreadTaskRunner> pending_task_runner_;
  mutable scoped_refptr<base::SingleThreadTaskRunner> original_task_runner_;
  std::unique_ptr<base::TickClock> time_source_;

  DISALLOW_COPY_AND_ASSIGN(LazySchedulerMessageLoopDelegateForTests);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_TEST_LAZY_SCHEDULER_MESSAGE_LOOP_DELEGATE_FOR_TESTS_H_
