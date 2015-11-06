// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_MANAGER_DELEGATE_H_
#define COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_MANAGER_DELEGATE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/time/tick_clock.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

class SCHEDULER_EXPORT TaskQueueManagerDelegate
    : public base::SingleThreadTaskRunner,
      public base::TickClock {
 public:
  TaskQueueManagerDelegate() {}

  // Returns true if the task runner is nested (i.e., running a run loop within
  // a nested task).
  virtual bool IsNested() const = 0;

  // Used by the TaskQueueManager to tell us there is no more non-delayed work
  // to do.
  // TODO(alexclarke): Try and find a better interface that avoids the need for
  // this.
  virtual void OnNoMoreImmediateWork() = 0;

  // Returns the time as a double which is the number of seconds since epoch
  // (Jan 1, 1970).  Blink uses this format to represent time.
  virtual double CurrentTimeSeconds() const = 0;

 protected:
  ~TaskQueueManagerDelegate() override {}

  DISALLOW_COPY_AND_ASSIGN(TaskQueueManagerDelegate);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_MANAGER_DELEGATE_H_
