// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_SCHEDULER_MESSAGE_LOOP_DELEGATE_H_
#define COMPONENTS_SCHEDULER_CHILD_SCHEDULER_MESSAGE_LOOP_DELEGATE_H_

#include "base/message_loop/message_loop.h"
#include "components/scheduler/child/nestable_single_thread_task_runner.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

class SCHEDULER_EXPORT SchedulerMessageLoopDelegate
    : public NestableSingleThreadTaskRunner {
 public:
  // |message_loop| is not owned and must outlive the lifetime of this object.
  static scoped_refptr<SchedulerMessageLoopDelegate> Create(
      base::MessageLoop* message_loop);

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

 protected:
  ~SchedulerMessageLoopDelegate() override;

 private:
  SchedulerMessageLoopDelegate(base::MessageLoop* message_loop);

  // Not owned.
  base::MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerMessageLoopDelegate);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_SCHEDULER_MESSAGE_LOOP_DELEGATE_H_
