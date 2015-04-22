// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/prioritizing_task_queue_selector.h"

#include "base/logging.h"
#include "base/pending_task.h"
#include "base/trace_event/trace_event_argument.h"

namespace scheduler {

PrioritizingTaskQueueSelector::PrioritizingTaskQueueSelector()
    : starvation_count_(0), task_queue_selector_observer_(nullptr) {
}

PrioritizingTaskQueueSelector::~PrioritizingTaskQueueSelector() {
}

void PrioritizingTaskQueueSelector::RegisterWorkQueues(
    const std::vector<const base::TaskQueue*>& work_queues) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  work_queues_ = work_queues;
  for (auto& queue_priority : queue_priorities_) {
    queue_priority.clear();
  }

  // By default, all work queues are set to normal priority.
  for (size_t i = 0; i < work_queues.size(); i++) {
    queue_priorities_[NORMAL_PRIORITY].insert(i);
  }
}

void PrioritizingTaskQueueSelector::SetQueuePriority(size_t queue_index,
                                                     QueuePriority priority) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_LT(queue_index, work_queues_.size());
  DCHECK_LT(priority, QUEUE_PRIORITY_COUNT);
  bool previously_enabled = DisableQueueInternal(queue_index);
  queue_priorities_[priority].insert(queue_index);
  if (task_queue_selector_observer_ && !previously_enabled)
    task_queue_selector_observer_->OnTaskQueueEnabled();
}

void PrioritizingTaskQueueSelector::EnableQueue(size_t queue_index,
                                                QueuePriority priority) {
  SetQueuePriority(queue_index, priority);
}

void PrioritizingTaskQueueSelector::DisableQueue(size_t queue_index) {
  DisableQueueInternal(queue_index);
}

bool PrioritizingTaskQueueSelector::DisableQueueInternal(size_t queue_index) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_LT(queue_index, work_queues_.size());
  bool previously_enabled = false;
  for (auto& queue_priority : queue_priorities_) {
    if (queue_priority.erase(queue_index))
      previously_enabled = true;
  }
  return previously_enabled;
}

bool PrioritizingTaskQueueSelector::IsQueueEnabled(size_t queue_index) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_LT(queue_index, work_queues_.size());
  for (const auto& queue_priority : queue_priorities_) {
    if (queue_priority.find(queue_index) != queue_priority.end())
      return true;
  }
  return false;
}

bool PrioritizingTaskQueueSelector::IsOlder(const base::TaskQueue* queueA,
                                            const base::TaskQueue* queueB) {
  // Note: the comparison is correct due to the fact that the PendingTask
  // operator inverts its comparison operation in order to work well in a heap
  // based priority queue.
  return queueB->front() < queueA->front();
}

PrioritizingTaskQueueSelector::QueuePriority
PrioritizingTaskQueueSelector::NextPriority(QueuePriority priority) {
  DCHECK(priority < QUEUE_PRIORITY_COUNT);
  return static_cast<QueuePriority>(static_cast<int>(priority) + 1);
}

bool PrioritizingTaskQueueSelector::ChooseOldestWithPriority(
    QueuePriority priority,
    size_t* out_queue_index) const {
  bool found_non_empty_queue = false;
  size_t chosen_queue = 0;
  for (int queue_index : queue_priorities_[priority]) {
    if (work_queues_[queue_index]->empty()) {
      continue;
    }
    if (!found_non_empty_queue ||
        IsOlder(work_queues_[queue_index], work_queues_[chosen_queue])) {
      found_non_empty_queue = true;
      chosen_queue = queue_index;
    }
  }

  if (found_non_empty_queue) {
    *out_queue_index = chosen_queue;
  }
  return found_non_empty_queue;
}

bool PrioritizingTaskQueueSelector::SelectWorkQueueToService(
    size_t* out_queue_index) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(work_queues_.size());
  // Always service the control queue if it has any work.
  if (ChooseOldestWithPriority(CONTROL_PRIORITY, out_queue_index)) {
    DidSelectQueueWithPriority(CONTROL_PRIORITY);
    return true;
  }
  // Select from the normal priority queue if we are starving it.
  if (starvation_count_ >= kMaxStarvationTasks &&
      ChooseOldestWithPriority(NORMAL_PRIORITY, out_queue_index)) {
    DidSelectQueueWithPriority(NORMAL_PRIORITY);
    return true;
  }
  // Otherwise choose in priority order.
  for (QueuePriority priority = HIGH_PRIORITY; priority < QUEUE_PRIORITY_COUNT;
       priority = NextPriority(priority)) {
    if (ChooseOldestWithPriority(priority, out_queue_index)) {
      DidSelectQueueWithPriority(priority);
      return true;
    }
  }
  return false;
}

void PrioritizingTaskQueueSelector::DidSelectQueueWithPriority(
    QueuePriority priority) {
  switch (priority) {
    case CONTROL_PRIORITY:
      break;
    case HIGH_PRIORITY:
      starvation_count_++;
      break;
    case NORMAL_PRIORITY:
    case BEST_EFFORT_PRIORITY:
      starvation_count_ = 0;
      break;
    default:
      NOTREACHED();
  }
}

// static
const char* PrioritizingTaskQueueSelector::PriorityToString(
    QueuePriority priority) {
  switch (priority) {
    case CONTROL_PRIORITY:
      return "control";
    case HIGH_PRIORITY:
      return "high";
    case NORMAL_PRIORITY:
      return "normal";
    case BEST_EFFORT_PRIORITY:
      return "best_effort";
    default:
      NOTREACHED();
      return nullptr;
  }
}

void PrioritizingTaskQueueSelector::AsValueInto(
    base::trace_event::TracedValue* state) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  state->BeginDictionary("priorities");
  for (QueuePriority priority = FIRST_QUEUE_PRIORITY;
       priority < QUEUE_PRIORITY_COUNT; priority = NextPriority(priority)) {
    state->BeginArray(PriorityToString(priority));
    for (size_t queue_index : queue_priorities_[priority])
      state->AppendInteger(queue_index);
    state->EndArray();
  }
  state->EndDictionary();
  state->SetInteger("starvation_count", starvation_count_);
}

void PrioritizingTaskQueueSelector::SetTaskQueueSelectorObserver(
    Observer* observer) {
  task_queue_selector_observer_ = observer;
}

}  // namespace scheduler
