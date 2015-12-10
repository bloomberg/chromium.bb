// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/work_queue.h"

#include "components/scheduler/base/work_queue_sets.h"

namespace scheduler {
namespace internal {

WorkQueue::WorkQueue(WorkQueueSets* task_queue_sets,
                     TaskQueueImpl* task_queue,
                     const char* name)
    : work_queue_sets_(task_queue_sets),
      task_queue_(task_queue),
      work_queue_set_index_(0),
      name_(name) {}

void WorkQueue::AsValueInto(base::trace_event::TracedValue* state) const {
  std::queue<TaskQueueImpl::Task> queue_copy(work_queue_);
  while (!queue_copy.empty()) {
    TaskQueueImpl::TaskAsValueInto(queue_copy.front(), state);
    queue_copy.pop();
  }
}

WorkQueue::~WorkQueue() {}

void WorkQueue::Clear() {
  work_queue_ = std::queue<TaskQueueImpl::Task>();
}

bool WorkQueue::GetFrontTaskEnqueueOrder(EnqueueOrder* enqueue_order) const {
  if (work_queue_.empty())
    return false;
  *enqueue_order = work_queue_.front().enqueue_order();
  return true;
}

void WorkQueue::Push(const TaskQueueImpl::Task&& task) {
  bool was_empty = work_queue_.empty();
  work_queue_.push(task);
  if (was_empty)
    work_queue_sets_->OnPushQueue(this);
}

void WorkQueue::PushTaskForTest(const TaskQueueImpl::Task&& task) {
  work_queue_.push(task);
}

void WorkQueue::PushAndSetEnqueueOrder(const TaskQueueImpl::Task&& task,
                                       EnqueueOrder enqueue_order) {
  bool was_empty = work_queue_.empty();
  work_queue_.push(task);
  work_queue_.back().set_enqueue_order(enqueue_order);

  if (was_empty)
    work_queue_sets_->OnPushQueue(this);
}

void WorkQueue::PopTaskForTest() {
  work_queue_.pop();
}

void WorkQueue::SwapLocked(std::queue<TaskQueueImpl::Task>& incoming_queue) {
  std::swap(work_queue_, incoming_queue);

  if (!work_queue_.empty())
    work_queue_sets_->OnPushQueue(this);
  task_queue_->TraceQueueSize(true);
}

TaskQueueImpl::Task WorkQueue::TakeTaskFromWorkQueue() {
  DCHECK(!work_queue_.empty());
  TaskQueueImpl::Task pending_task = std::move(work_queue_.front());
  work_queue_.pop();
  work_queue_sets_->OnPopQueue(this);
  task_queue_->TraceQueueSize(false);
  return pending_task;
}

}  // namespace internal
}  // namespace scheduler
