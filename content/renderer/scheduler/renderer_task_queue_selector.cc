// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/renderer_task_queue_selector.h"

#include "base/logging.h"
#include "base/pending_task.h"
#include "base/trace_event/trace_event_argument.h"

namespace content {

RendererTaskQueueSelector::RendererTaskQueueSelector() : starvation_count_(0) {
}

RendererTaskQueueSelector::~RendererTaskQueueSelector() {
}

void RendererTaskQueueSelector::RegisterWorkQueues(
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

void RendererTaskQueueSelector::SetQueuePriority(size_t queue_index,
                                                 QueuePriority priority) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_LT(queue_index, work_queues_.size());
  DCHECK_LT(priority, QUEUE_PRIORITY_COUNT);
  DisableQueue(queue_index);
  queue_priorities_[priority].insert(queue_index);
}

void RendererTaskQueueSelector::EnableQueue(size_t queue_index,
                                            QueuePriority priority) {
  SetQueuePriority(queue_index, priority);
}

void RendererTaskQueueSelector::DisableQueue(size_t queue_index) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_LT(queue_index, work_queues_.size());
  for (auto& queue_priority : queue_priorities_) {
    queue_priority.erase(queue_index);
  }
}

bool RendererTaskQueueSelector::IsQueueEnabled(size_t queue_index) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_LT(queue_index, work_queues_.size());
  for (const auto& queue_priority : queue_priorities_) {
    if (queue_priority.find(queue_index) != queue_priority.end())
      return true;
  }
  return false;
}

bool RendererTaskQueueSelector::IsOlder(const base::TaskQueue* queueA,
                                        const base::TaskQueue* queueB) {
  // Note: the comparison is correct due to the fact that the PendingTask
  // operator inverts its comparison operation in order to work well in a heap
  // based priority queue.
  return queueB->front() < queueA->front();
}

RendererTaskQueueSelector::QueuePriority
RendererTaskQueueSelector::NextPriority(QueuePriority priority) {
  DCHECK(priority < QUEUE_PRIORITY_COUNT);
  return static_cast<QueuePriority>(static_cast<int>(priority) + 1);
}

bool RendererTaskQueueSelector::ChooseOldestWithPriority(
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

bool RendererTaskQueueSelector::SelectWorkQueueToService(
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

void RendererTaskQueueSelector::DidSelectQueueWithPriority(
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
const char* RendererTaskQueueSelector::PriorityToString(
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

void RendererTaskQueueSelector::AsValueInto(
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

}  // namespace content
