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
    : enabled_selector_(this),
      blocked_selector_(this),
      immediate_starvation_count_(0),
      high_priority_starvation_count_(0),
      num_blocked_queues_to_report_(0),
      task_queue_selector_observer_(nullptr) {}

TaskQueueSelector::~TaskQueueSelector() {}

void TaskQueueSelector::AddQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(queue->IsQueueEnabled());
  SetQueuePriority(queue, TaskQueue::NORMAL_PRIORITY);
}

void TaskQueueSelector::RemoveQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (queue->IsQueueEnabled()) {
    enabled_selector_.RemoveQueue(queue);
  } else if (queue->should_report_when_execution_blocked()) {
    DCHECK_GT(num_blocked_queues_to_report_, 0u);
    num_blocked_queues_to_report_--;
    blocked_selector_.RemoveQueue(queue);
  }
}

void TaskQueueSelector::EnableQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(queue->IsQueueEnabled());
  if (queue->should_report_when_execution_blocked()) {
    DCHECK_GT(num_blocked_queues_to_report_, 0u);
    num_blocked_queues_to_report_--;
  }
  blocked_selector_.RemoveQueue(queue);
  enabled_selector_.AssignQueueToSet(queue, queue->GetQueuePriority());
  if (task_queue_selector_observer_)
    task_queue_selector_observer_->OnTaskQueueEnabled(queue);
}

void TaskQueueSelector::DisableQueue(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(!queue->IsQueueEnabled());
  enabled_selector_.RemoveQueue(queue);
  blocked_selector_.AssignQueueToSet(queue, queue->GetQueuePriority());
  if (queue->should_report_when_execution_blocked())
    num_blocked_queues_to_report_++;
}

void TaskQueueSelector::SetQueuePriority(internal::TaskQueueImpl* queue,
                                         TaskQueue::QueuePriority priority) {
  DCHECK_LT(priority, TaskQueue::QUEUE_PRIORITY_COUNT);
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (queue->IsQueueEnabled()) {
    enabled_selector_.AssignQueueToSet(queue, priority);
  } else {
    blocked_selector_.AssignQueueToSet(queue, priority);
  }
  DCHECK_EQ(priority, queue->GetQueuePriority());
}

TaskQueue::QueuePriority TaskQueueSelector::NextPriority(
    TaskQueue::QueuePriority priority) {
  DCHECK(priority < TaskQueue::QUEUE_PRIORITY_COUNT);
  return static_cast<TaskQueue::QueuePriority>(static_cast<int>(priority) + 1);
}

TaskQueueSelector::PrioritizingSelector::PrioritizingSelector(
    TaskQueueSelector* task_queue_selector)
    : task_queue_selector_(task_queue_selector),
      delayed_work_queue_sets_(TaskQueue::QUEUE_PRIORITY_COUNT),
      immediate_work_queue_sets_(TaskQueue::QUEUE_PRIORITY_COUNT) {}

void TaskQueueSelector::PrioritizingSelector::AssignQueueToSet(
    internal::TaskQueueImpl* queue,
    TaskQueue::QueuePriority priority) {
  delayed_work_queue_sets_.AssignQueueToSet(queue->delayed_work_queue(),
                                            priority);
  immediate_work_queue_sets_.AssignQueueToSet(queue->immediate_work_queue(),
                                              priority);
}

void TaskQueueSelector::PrioritizingSelector::RemoveQueue(
    internal::TaskQueueImpl* queue) {
  delayed_work_queue_sets_.RemoveQueue(queue->delayed_work_queue());
  immediate_work_queue_sets_.RemoveQueue(queue->immediate_work_queue());
}

bool TaskQueueSelector::PrioritizingSelector::
    ChooseOldestImmediateTaskWithPriority(TaskQueue::QueuePriority priority,
                                          WorkQueue** out_work_queue) const {
  return immediate_work_queue_sets_.GetOldestQueueInSet(priority,
                                                        out_work_queue);
}

bool TaskQueueSelector::PrioritizingSelector::
    ChooseOldestDelayedTaskWithPriority(TaskQueue::QueuePriority priority,
                                        WorkQueue** out_work_queue) const {
  return delayed_work_queue_sets_.GetOldestQueueInSet(priority, out_work_queue);
}

bool TaskQueueSelector::PrioritizingSelector::
    ChooseOldestImmediateOrDelayedTaskWithPriority(
        TaskQueue::QueuePriority priority,
        bool* out_chose_delayed_over_immediate,
        WorkQueue** out_work_queue) const {
  WorkQueue* immediate_queue;
  if (immediate_work_queue_sets_.GetOldestQueueInSet(priority,
                                                     &immediate_queue)) {
    WorkQueue* delayed_queue;
    if (delayed_work_queue_sets_.GetOldestQueueInSet(priority,
                                                     &delayed_queue)) {
      if (immediate_queue->ShouldRunBefore(delayed_queue)) {
        *out_work_queue = immediate_queue;
      } else {
        *out_chose_delayed_over_immediate = true;
        *out_work_queue = delayed_queue;
      }
    } else {
      *out_work_queue = immediate_queue;
    }
    return true;
  }
  return delayed_work_queue_sets_.GetOldestQueueInSet(priority, out_work_queue);
}

bool TaskQueueSelector::PrioritizingSelector::ChooseOldestWithPriority(
    TaskQueue::QueuePriority priority,
    bool* out_chose_delayed_over_immediate,
    WorkQueue** out_work_queue) const {
  // Select an immediate work queue if we are starving immediate tasks.
  if (task_queue_selector_->immediate_starvation_count_ >=
      kMaxDelayedStarvationTasks) {
    if (ChooseOldestImmediateTaskWithPriority(priority, out_work_queue)) {
      return true;
    }
    if (ChooseOldestDelayedTaskWithPriority(priority, out_work_queue)) {
      return true;
    }
    return false;
  }
  return ChooseOldestImmediateOrDelayedTaskWithPriority(
      priority, out_chose_delayed_over_immediate, out_work_queue);
}

bool TaskQueueSelector::PrioritizingSelector::SelectWorkQueueToService(
    TaskQueue::QueuePriority max_priority,
    WorkQueue** out_work_queue,
    bool* out_chose_delayed_over_immediate) {
  DCHECK(task_queue_selector_->main_thread_checker_.CalledOnValidThread());

  // Always service the control queue if it has any work.
  if (max_priority > TaskQueue::CONTROL_PRIORITY &&
      ChooseOldestWithPriority(TaskQueue::CONTROL_PRIORITY,
                               out_chose_delayed_over_immediate,
                               out_work_queue)) {
    return true;
  }

  // Select from the normal priority queue if we are starving it.
  if (max_priority > TaskQueue::NORMAL_PRIORITY &&
      task_queue_selector_->high_priority_starvation_count_ >=
          kMaxHighPriorityStarvationTasks &&
      ChooseOldestWithPriority(TaskQueue::NORMAL_PRIORITY,
                               out_chose_delayed_over_immediate,
                               out_work_queue)) {
    return true;
  }
  // Otherwise choose in priority order.
  for (TaskQueue::QueuePriority priority = TaskQueue::HIGH_PRIORITY;
       priority < max_priority; priority = NextPriority(priority)) {
    if (ChooseOldestWithPriority(priority, out_chose_delayed_over_immediate,
                                 out_work_queue)) {
      return true;
    }
  }
  return false;
}

bool TaskQueueSelector::SelectWorkQueueToService(WorkQueue** out_work_queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  bool chose_delayed_over_immediate = false;
  bool found_queue = enabled_selector_.SelectWorkQueueToService(
      TaskQueue::QUEUE_PRIORITY_COUNT, out_work_queue,
      &chose_delayed_over_immediate);
  if (!found_queue) {
    TrySelectingBlockedQueue(nullptr);
    return false;
  }

  TrySelectingBlockedQueue(*out_work_queue);
  DidSelectQueueWithPriority(
      (*out_work_queue)->task_queue()->GetQueuePriority(),
      chose_delayed_over_immediate);
  return true;
}

void TaskQueueSelector::TrySelectingBlockedQueue(
    WorkQueue* chosen_enabled_queue) {
  if (!num_blocked_queues_to_report_ || !task_queue_selector_observer_)
    return;

  TaskQueue::QueuePriority max_priority = TaskQueue::QUEUE_PRIORITY_COUNT;
  if (chosen_enabled_queue) {
    max_priority =
        NextPriority(chosen_enabled_queue->task_queue()->GetQueuePriority());
  }

  WorkQueue* chosen_blocked_queue;
  bool chose_delayed_over_immediate = false;
  bool found_queue = blocked_selector_.SelectWorkQueueToService(
      max_priority, &chosen_blocked_queue, &chose_delayed_over_immediate);
  if (!found_queue)
    return;

  // If we did not choose a normal task or chose one at a lower priority than a
  // potential blocked task, we would have run the blocked task instead.
  if (!chosen_enabled_queue ||
      chosen_blocked_queue->task_queue()->GetQueuePriority() <
          chosen_enabled_queue->task_queue()->GetQueuePriority()) {
    task_queue_selector_observer_->OnTriedToSelectBlockedWorkQueue(
        chosen_blocked_queue);
    return;
  }
  // Otherwise there was an enabled and a blocked task with the same priority.
  // The one with the older enqueue order wins.
  if (chosen_blocked_queue->ShouldRunBefore(chosen_enabled_queue)) {
    task_queue_selector_observer_->OnTriedToSelectBlockedWorkQueue(
        chosen_blocked_queue);
  }
}

void TaskQueueSelector::DidSelectQueueWithPriority(
    TaskQueue::QueuePriority priority,
    bool chose_delayed_over_immediate) {
  switch (priority) {
    case TaskQueue::CONTROL_PRIORITY:
      break;
    case TaskQueue::HIGH_PRIORITY:
      high_priority_starvation_count_++;
      break;
    case TaskQueue::NORMAL_PRIORITY:
    case TaskQueue::BEST_EFFORT_PRIORITY:
      high_priority_starvation_count_ = 0;
      break;
    default:
      NOTREACHED();
  }
  if (chose_delayed_over_immediate) {
    immediate_starvation_count_++;
  } else {
    immediate_starvation_count_ = 0;
  }
}

void TaskQueueSelector::AsValueInto(
    base::trace_event::TracedValue* state) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  state->SetInteger("high_priority_starvation_count",
                    high_priority_starvation_count_);
  state->SetInteger("immediate_starvation_count", immediate_starvation_count_);
  state->SetInteger("num_blocked_queues_to_report",
                    num_blocked_queues_to_report_);
}

void TaskQueueSelector::SetTaskQueueSelectorObserver(Observer* observer) {
  task_queue_selector_observer_ = observer;
}

bool TaskQueueSelector::EnabledWorkQueuesEmpty() const {
  for (TaskQueue::QueuePriority priority = TaskQueue::CONTROL_PRIORITY;
       priority < TaskQueue::QUEUE_PRIORITY_COUNT;
       priority = NextPriority(priority)) {
    if (!enabled_selector_.delayed_work_queue_sets()->IsSetEmpty(priority) ||
        !enabled_selector_.immediate_work_queue_sets()->IsSetEmpty(priority)) {
      return false;
    }
  }
  return true;
}

void TaskQueueSelector::SetImmediateStarvationCountForTest(
    size_t immediate_starvation_count) {
  immediate_starvation_count_ = immediate_starvation_count;
}

}  // namespace internal
}  // namespace scheduler
