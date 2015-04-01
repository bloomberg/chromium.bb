// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SCHEDULER_SCHEDULER_MESSAGE_LOOP_DELEGATE_H_
#define CONTENT_CHILD_SCHEDULER_SCHEDULER_MESSAGE_LOOP_DELEGATE_H_

#include "base/message_loop/message_loop.h"
#include "content/child/scheduler/nestable_single_thread_task_runner.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT SchedulerMessageLoopDelegate
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

 protected:
  ~SchedulerMessageLoopDelegate() override;

 private:
  SchedulerMessageLoopDelegate(base::MessageLoop* message_loop);

  // Not owned.
  base::MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerMessageLoopDelegate);
};

}  // namespace content

#endif  // CONTENT_CHILD_SCHEDULER_SCHEDULER_MESSAGE_LOOP_DELEGATE_H_
