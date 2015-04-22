// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_TASK_QUEUE_SELECTOR_H_
#define COMPONENTS_SCHEDULER_CHILD_TASK_QUEUE_SELECTOR_H_

#include <vector>

#include "base/basictypes.h"
#include "components/scheduler/scheduler_export.h"

namespace base {
class TaskQueue;
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace scheduler {

class TaskQueueSelector {
 public:
  virtual ~TaskQueueSelector() {}

  // Called once to register the work queues to be selected from. This function
  // is called on the main thread.
  virtual void RegisterWorkQueues(
      const std::vector<const base::TaskQueue*>& work_queues) = 0;

  class SCHEDULER_EXPORT Observer {
   public:
    virtual ~Observer() {}

    // Called when a task queue transitions from disabled to enabled.
    virtual void OnTaskQueueEnabled() = 0;
  };

  // Called once to set the Observer. This function is called
  // on the main thread. If |observer| is null, then no callbacks will occur.
  virtual void SetTaskQueueSelectorObserver(Observer* observer) = 0;

  // Called to choose the work queue from which the next task should be taken
  // and run. Return true if |out_queue| indicates the queue to service or
  // false to avoid running any task.
  //
  // This function is called on the main thread.
  virtual bool SelectWorkQueueToService(size_t* out_queue_index) = 0;

  // Serialize the selector state for tracing.
  virtual void AsValueInto(base::trace_event::TracedValue* state) const = 0;
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_TASK_QUEUE_SELECTOR_H_
