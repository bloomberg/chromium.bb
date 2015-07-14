// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_TEST_LAZY_SCHEDULER_MESSAGE_LOOP_DELEGATE_FOR_TESTS_H_
#define COMPONENTS_SCHEDULER_TEST_LAZY_SCHEDULER_MESSAGE_LOOP_DELEGATE_FOR_TESTS_H_

#include "base/message_loop/message_loop.h"
#include "components/scheduler/child/nestable_single_thread_task_runner.h"

namespace scheduler {

// This class connects the scheduler to a MessageLoop, but unlike
// SchedulerMessageLoopDelegate it allows the message loop to be created lazily
// after the scheduler has been brought up. This is needed in testing scenarios
// where Blink is initialized before a MessageLoop has been created.
//
// TODO(skyostil): Fix the relevant test suites and remove this class
// (crbug.com/495659).
class LazySchedulerMessageLoopDelegateForTests
    : public NestableSingleThreadTaskRunner {
 public:
  static scoped_refptr<LazySchedulerMessageLoopDelegateForTests> Create();

  // NestableSingleThreadTaskRunner implementation
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override;
  bool RunsTasksOnCurrentThread() const override;
  bool IsNested() const override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;

 private:
  LazySchedulerMessageLoopDelegateForTests();
  ~LazySchedulerMessageLoopDelegateForTests() override;

  bool HasMessageLoop() const;
  base::MessageLoop* EnsureMessageLoop() const;

  mutable base::MessageLoop* message_loop_;
  base::PlatformThreadId thread_id_;

  // Task observers which have not yet been registered to a message loop. Not
  // owned.
  mutable base::hash_set<base::MessageLoop::TaskObserver*> pending_observers_;

  DISALLOW_COPY_AND_ASSIGN(LazySchedulerMessageLoopDelegateForTests);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_TEST_LAZY_SCHEDULER_MESSAGE_LOOP_DELEGATE_FOR_TESTS_H_
