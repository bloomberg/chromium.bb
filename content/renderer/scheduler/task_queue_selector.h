// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_TASK_QUEUE_SELECTOR_H_
#define CONTENT_RENDERER_SCHEDULER_TASK_QUEUE_SELECTOR_H_

#include <vector>

#include "base/basictypes.h"

namespace base {
class TaskQueue;
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace content {

class TaskQueueSelector {
 public:
  virtual ~TaskQueueSelector() {}

  // Called once to register the work queues to be selected from. This function
  // is called on the main thread.
  virtual void RegisterWorkQueues(
      const std::vector<const base::TaskQueue*>& work_queues) = 0;

  // Called to choose the work queue from which the next task should be taken
  // and run. Return true if |out_queue| indicates the queue to service or
  // false to avoid running any task.
  //
  // This function is called on the main thread.
  virtual bool SelectWorkQueueToService(size_t* out_queue_index) = 0;

  // Serialize the selector state for tracing.
  virtual void AsValueInto(base::trace_event::TracedValue* state) const = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCHEDULER_TASK_QUEUE_SELECTOR_H_
