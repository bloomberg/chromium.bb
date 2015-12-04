// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/task_queue_selector.h"

#include "base/logging.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/work_queue.h"

namespace scheduler {
namespace internal {

TaskQueueSelector::TaskQueueSelector()
    : delayed_work_queue_sets_(TaskQueue::QUEUE_PRIORITY_COUNT),
      immediate_work_queue_sets_(TaskQueue::QUEUE_PRIORITY_COUNT),
      force_select_immediate_(true),
      starvation_count_(0),
      task_queue_selector_observer_(nullptr) {}

TaskQueueSelector::~TaskQueueSelector() {}

void TaskQueueSelector::AddQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  delayed_work_queue_sets_.AssignQueueToSet(queue->delayed_work_queue(),
                                            TaskQueue::NORMAL_PRIORITY);
  immediate_work_queue_sets_.AssignQueueToSet(queue->immediate_work_queue(),
                                              TaskQueue::NORMAL_PRIORITY);
}

void TaskQueueSelector::RemoveQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  delayed_work_queue_sets_.RemoveQueue(queue->delayed_work_queue());
  immediate_work_queue_sets_.RemoveQueue(queue->immediate_work_queue());
}

void TaskQueueSelector::SetQueuePriority(internal::TaskQueueImpl* queue,
                                         TaskQueue::QueuePriority priority) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_LT(priority, TaskQueue::QUEUE_PRIORITY_COUNT);
  int old_set_index = queue->immediate_work_queue()->work_queue_set_index();
  TaskQueue::QueuePriority old_priority =
      static_cast<TaskQueue::QueuePriority>(old_set_index);
  delayed_work_queue_sets_.AssignQueueToSet(queue->delayed_work_queue(),
                                            priority);
  immediate_work_queue_sets_.AssignQueueToSet(queue->immediate_work_queue(),
                                              priority);
  if (task_queue_selector_observer_ &&
      old_priority == TaskQueue::DISABLED_PRIORITY) {
    task_queue_selector_observer_->OnTaskQueueEnabled(queue);
  }
}

bool TaskQueueSelector::IsQueueEnabled(
    const internal::TaskQueueImpl* queue) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  size_t set_index = queue->delayed_work_queue()->work_queue_set_index();
  return static_cast<TaskQueue::QueuePriority>(set_index) !=
         TaskQueue::DISABLED_PRIORITY;
}

TaskQueue::QueuePriority TaskQueueSelector::NextPriority(
    TaskQueue::QueuePriority priority) {
  DCHECK(priority < TaskQueue::QUEUE_PRIORITY_COUNT);
  return static_cast<TaskQueue::QueuePriority>(static_cast<int>(priority) + 1);
}

bool TaskQueueSelector::ChooseOldestImmediateTaskWithPriority(
    TaskQueue::QueuePriority priority,
    WorkQueue** out_work_queue) const {
  return immediate_work_queue_sets_.GetOldestQueueInSet(priority,
                                                        out_work_queue);
}

bool TaskQueueSelector::ChooseOldestDelayedTaskWithPriority(
    TaskQueue::QueuePriority priority,
    WorkQueue** out_work_queue) const {
  return delayed_work_queue_sets_.GetOldestQueueInSet(priority, out_work_queue);
}

bool TaskQueueSelector::ChooseOldestImmediateOrDelayedTaskWithPriority(
    TaskQueue::QueuePriority priority,
    WorkQueue** out_work_queue) const {
  WorkQueue* immediate_queue;
  if (immediate_work_queue_sets_.GetOldestQueueInSet(priority,
                                                     &immediate_queue)) {
    WorkQueue* delayed_queue;
    if (delayed_work_queue_sets_.GetOldestQueueInSet(priority,
                                                     &delayed_queue)) {
      int immediate_enqueue_order;
      int delayed_enqueue_order;
      bool have_immediate_task =
          immediate_queue->GetFrontTaskEnqueueOrder(&immediate_enqueue_order);
      bool have_delayed_task =
          delayed_queue->GetFrontTaskEnqueueOrder(&delayed_enqueue_order);
      DCHECK(have_immediate_task);
      DCHECK(have_delayed_task);
      if (immediate_enqueue_order < delayed_enqueue_order) {
        *out_work_queue = immediate_queue;
      } else {
        *out_work_queue = delayed_queue;
      }
    } else {
      *out_work_queue = immediate_queue;
    }
    return true;
  } else {
    if (delayed_work_queue_sets_.GetOldestQueueInSet(priority,
                                                     out_work_queue)) {
      return true;
    } else {
      return false;
    }
  }
}

bool TaskQueueSelector::ChooseOldestWithPriority(
    TaskQueue::QueuePriority priority,
    WorkQueue** out_work_queue) const {
  if (force_select_immediate_) {
    if (ChooseOldestImmediateTaskWithPriority(priority, out_work_queue)) {
      return true;
    }
    if (ChooseOldestDelayedTaskWithPriority(priority, out_work_queue)) {
      return true;
    }
    return false;
  } else {
    return ChooseOldestImmediateOrDelayedTaskWithPriority(priority,
                                                          out_work_queue);
  }
}

bool TaskQueueSelector::SelectWorkQueueToService(WorkQueue** out_work_queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  force_select_immediate_ = !force_select_immediate_;
  // Always service the control queue if it has any work.
  if (ChooseOldestWithPriority(TaskQueue::CONTROL_PRIORITY, out_work_queue)) {
    DidSelectQueueWithPriority(TaskQueue::CONTROL_PRIORITY);
    return true;
  }
  // Select from the normal priority queue if we are starving it.
  if (starvation_count_ >= kMaxStarvationTasks &&
      ChooseOldestWithPriority(TaskQueue::NORMAL_PRIORITY, out_work_queue)) {
    DidSelectQueueWithPriority(TaskQueue::NORMAL_PRIORITY);
    return true;
  }
  // Otherwise choose in priority order.
  for (TaskQueue::QueuePriority priority = TaskQueue::HIGH_PRIORITY;
       priority < TaskQueue::DISABLED_PRIORITY;
       priority = NextPriority(priority)) {
    if (ChooseOldestWithPriority(priority, out_work_queue)) {
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
  state->SetBoolean("try_delayed_first", force_select_immediate_);
}

void TaskQueueSelector::SetTaskQueueSelectorObserver(Observer* observer) {
  task_queue_selector_observer_ = observer;
}

bool TaskQueueSelector::EnabledWorkQueuesEmpty() const {
  for (TaskQueue::QueuePriority priority = TaskQueue::HIGH_PRIORITY;
       priority < TaskQueue::DISABLED_PRIORITY;
       priority = NextPriority(priority)) {
    if (!delayed_work_queue_sets_.IsSetEmpty(priority) ||
        !immediate_work_queue_sets_.IsSetEmpty(priority)) {
      return false;
    }
  }
  return true;
}

void TaskQueueSelector::SetForceSelectImmediateForTest(
    bool force_select_immediate) {
  force_select_immediate_ = force_select_immediate;
}

}  // namespace internal
}  // namespace scheduler
