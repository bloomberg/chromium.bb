// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_SELECTOR_H_
#define COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_SELECTOR_H_

#include <set>

#include "base/compiler_specific.h"
#include "base/pending_task.h"
#include "base/threading/thread_checker.h"
#include "components/scheduler/base/task_queue_sets.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {
namespace internal {

// TaskQueueSelector is used by the SchedulerHelper to enable prioritization
// of particular task queues.
class SCHEDULER_EXPORT TaskQueueSelector {
 public:
  TaskQueueSelector();
  ~TaskQueueSelector();

  // Set the priority of |queue| to |priority|.
  void SetQueuePriority(internal::TaskQueueImpl* queue,
                        TaskQueue::QueuePriority priority);

  // Whether |queue| is enabled.
  bool IsQueueEnabled(const internal::TaskQueueImpl* queue) const;

  // Called to register a queue that can be selected. This function is called
  // on the main thread.
  void AddQueue(internal::TaskQueueImpl* queue);

  // The specified work will no longer be considered for selection. This
  // function is called on the main thread.
  void RemoveQueue(internal::TaskQueueImpl* queue);

  // Called to choose the work queue from which the next task should be taken
  // and run. Return true if |out_queue| indicates the queue to service or
  // false to avoid running any task.
  //
  // This function is called on the main thread.
  bool SelectQueueToService(internal::TaskQueueImpl** out_queue);

  // Serialize the selector state for tracing.
  void AsValueInto(base::trace_event::TracedValue* state) const;

  class SCHEDULER_EXPORT Observer {
   public:
    virtual ~Observer() {}

    // Called when |queue| transitions from disabled to enabled.
    virtual void OnTaskQueueEnabled(internal::TaskQueueImpl* queue) = 0;
  };

  // Called once to set the Observer. This function is called
  // on the main thread. If |observer| is null, then no callbacks will occur.
  void SetTaskQueueSelectorObserver(Observer* observer);

  TaskQueueSets* GetTaskQueueSets() { return &task_queue_sets_; }

  // Returns true if all the enabled work queues are empty. Returns false
  // otherwise.
  bool EnabledWorkQueuesEmpty() const;

 private:
  // Returns the priority which is next after |priority|.
  static TaskQueue::QueuePriority NextPriority(
      TaskQueue::QueuePriority priority);

  // Return true if |out_queue| contains the queue with the oldest pending task
  // from the set of queues of |priority|, or false if all queues of that
  // priority are empty.
  bool ChooseOldestWithPriority(TaskQueue::QueuePriority priority,
                                internal::TaskQueueImpl** out_queue) const;

  // Called whenever the selector chooses a task queue for execution with the
  // priority |priority|.
  void DidSelectQueueWithPriority(TaskQueue::QueuePriority priority);

  // Number of high priority tasks which can be run before a normal priority
  // task should be selected to prevent starvation.
  // TODO(rmcilroy): Check if this is a good value.
  static const size_t kMaxStarvationTasks = 5;

  base::ThreadChecker main_thread_checker_;
  TaskQueueSets task_queue_sets_;

  size_t starvation_count_;
  Observer* task_queue_selector_observer_;  // NOT OWNED
  DISALLOW_COPY_AND_ASSIGN(TaskQueueSelector);
};

}  // namespace internal
}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_SELECTOR_H
