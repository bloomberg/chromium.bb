// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_BASE_WORK_QUEUE_H_
#define CONTENT_RENDERER_SCHEDULER_BASE_WORK_QUEUE_H_

#include <stddef.h>

#include <set>

#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/base/enqueue_order.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {
namespace internal {
class WorkQueueSets;

class SCHEDULER_EXPORT WorkQueue {
 public:
  WorkQueue(WorkQueueSets* task_queue_sets,
            TaskQueueImpl* task_queue,
            const char* name);
  ~WorkQueue();

  void AsValueInto(base::trace_event::TracedValue* state) const;

  // Clears the |work_queue_|.
  void Clear();

  // returns true if the |work_queue_| is empty.
  bool Empty() const { return work_queue_.empty(); }

  // If the |work_queue_| isn't empty, |enqueue_order| gets set to the enqueue
  // order of the front task and the function returns true.  Otherwise the
  // function returns false.
  bool GetFrontTaskEnqueueOrder(EnqueueOrder* enqueue_order) const;

  // Pushes the task onto the |work_queue_| and informs the WorkQueueSets if
  // the head changed.
  void Push(const TaskQueueImpl::Task&& task);

  // Pushes the task onto the |work_queue_|, sets the |enqueue_order| and
  // informs the WorkQueueSets if the head changed.
  void PushAndSetEnqueueOrder(const TaskQueueImpl::Task&& task,
                              EnqueueOrder enqueue_order);

  // Swap the |work_queue_| with |incoming_queue| and informs the
  // WorkQueueSets if the head changed. Assumes |task_queue_->any_thread_lock_|
  // is locked.
  void SwapLocked(std::queue<TaskQueueImpl::Task>& incoming_queue);

  size_t Size() const { return work_queue_.size(); }

  // Pulls a task off the |work_queue_| and informs the WorkQueueSets.
  TaskQueueImpl::Task TakeTaskFromWorkQueue();

  const char* name() const { return name_; }

  TaskQueueImpl* task_queue() const { return task_queue_; }

  size_t work_queue_set_index() const { return work_queue_set_index_; }

  void set_work_queue_set_index(size_t work_queue_set_index) {
    work_queue_set_index_ = work_queue_set_index;
  }

  // Test support functions.  These should not be used in production code.
  void PopTaskForTest();
  void PushTaskForTest(const TaskQueueImpl::Task&& task);

 private:
  std::queue<TaskQueueImpl::Task> work_queue_;
  WorkQueueSets* work_queue_sets_;  // NOT OWNED.
  TaskQueueImpl* task_queue_;       // NOT OWNED.
  size_t work_queue_set_index_;
  const char* name_;
};

}  // namespace internal
}  // namespace scheduler

#endif  // CONTENT_RENDERER_SCHEDULER_BASE_WORK_QUEUE_H_
