// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/task_queue_selector.h"

#include "base/logging.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/base/task_queue_impl.h"

namespace scheduler {
namespace internal {

TaskQueueSelector::TaskQueueSelector()
    : task_queue_sets_(TaskQueue::QUEUE_PRIORITY_COUNT),
      starvation_count_(0),
      task_queue_selector_observer_(nullptr) {}

TaskQueueSelector::~TaskQueueSelector() {}

void TaskQueueSelector::AddQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  task_queue_sets_.AssignQueueToSet(queue, TaskQueue::NORMAL_PRIORITY);
}

void TaskQueueSelector::RemoveQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  task_queue_sets_.RemoveQueue(queue);
}

void TaskQueueSelector::SetQueuePriority(internal::TaskQueueImpl* queue,
                                         TaskQueue::QueuePriority priority) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_LT(priority, TaskQueue::QUEUE_PRIORITY_COUNT);
  TaskQueue::QueuePriority old_priority =
      static_cast<TaskQueue::QueuePriority>(queue->get_task_queue_set_index());
  task_queue_sets_.AssignQueueToSet(queue, priority);
  if (task_queue_selector_observer_ &&
      old_priority == TaskQueue::DISABLED_PRIORITY) {
    task_queue_selector_observer_->OnTaskQueueEnabled(queue);
  }
}

bool TaskQueueSelector::IsQueueEnabled(
    const internal::TaskQueueImpl* queue) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  return static_cast<TaskQueue::QueuePriority>(
             queue->get_task_queue_set_index()) != TaskQueue::DISABLED_PRIORITY;
}

TaskQueue::QueuePriority TaskQueueSelector::NextPriority(
    TaskQueue::QueuePriority priority) {
  DCHECK(priority < TaskQueue::QUEUE_PRIORITY_COUNT);
  return static_cast<TaskQueue::QueuePriority>(static_cast<int>(priority) + 1);
}

bool TaskQueueSelector::ChooseOldestWithPriority(
    TaskQueue::QueuePriority priority,
    internal::TaskQueueImpl** out_queue) const {
  return task_queue_sets_.GetOldestQueueInSet(priority, out_queue);
}

bool TaskQueueSelector::SelectQueueToService(
    internal::TaskQueueImpl** out_queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // Always service the control queue if it has any work.
  if (ChooseOldestWithPriority(TaskQueue::CONTROL_PRIORITY, out_queue)) {
    DidSelectQueueWithPriority(TaskQueue::CONTROL_PRIORITY);
    return true;
  }
  // Select from the normal priority queue if we are starving it.
  if (starvation_count_ >= kMaxStarvationTasks &&
      ChooseOldestWithPriority(TaskQueue::NORMAL_PRIORITY, out_queue)) {
    DidSelectQueueWithPriority(TaskQueue::NORMAL_PRIORITY);
    return true;
  }
  // Otherwise choose in priority order.
  for (TaskQueue::QueuePriority priority = TaskQueue::HIGH_PRIORITY;
       priority < TaskQueue::DISABLED_PRIORITY;
       priority = NextPriority(priority)) {
    if (ChooseOldestWithPriority(priority, out_queue)) {
      DidSelectQueueWithPriority(priority);
      return true;
    }
  }
  return false;
}

void TaskQueueSelector::DidSelectQueueWithPriority(
    TaskQueue::QueuePriority priority) {
  switch (priority) {
    case TaskQueue::CONTROL_PRIORITY:
      break;
    case TaskQueue::HIGH_PRIORITY:
      starvation_count_++;
      break;
    case TaskQueue::NORMAL_PRIORITY:
    case TaskQueue::BEST_EFFORT_PRIORITY:
      starvation_count_ = 0;
      break;
    default:
      NOTREACHED();
  }
}

void TaskQueueSelector::AsValueInto(
    base::trace_event::TracedValue* state) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  state->SetInteger("starvation_count", starvation_count_);
}

void TaskQueueSelector::SetTaskQueueSelectorObserver(Observer* observer) {
  task_queue_selector_observer_ = observer;
}

bool TaskQueueSelector::EnabledWorkQueuesEmpty() const {
  for (TaskQueue::QueuePriority priority = TaskQueue::HIGH_PRIORITY;
       priority < TaskQueue::DISABLED_PRIORITY;
       priority = NextPriority(priority)) {
    if (!task_queue_sets_.IsSetEmpty(priority))
      return false;
  }
  return true;
}

}  // namespace internal
}  // namespace scheduler
