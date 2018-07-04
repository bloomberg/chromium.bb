// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/work_queue.h"

#include "base/task/sequence_manager/work_queue_sets.h"

namespace base {
namespace sequence_manager {
namespace internal {

WorkQueue::WorkQueue(TaskQueueImpl* task_queue,
                     const char* name,
                     QueueType queue_type)
    : task_queue_(task_queue), name_(name), queue_type_(queue_type) {}

void WorkQueue::AsValueInto(TimeTicks now,
                            trace_event::TracedValue* state) const {
  for (const TaskQueueImpl::Task& task : tasks_) {
    TaskQueueImpl::TaskAsValueInto(task, now, state);
  }
}

WorkQueue::~WorkQueue() {
  DCHECK(!work_queue_sets_) << task_queue_->GetName() << " : "
                            << work_queue_sets_->GetName() << " : " << name_;
}

const TaskQueueImpl::Task* WorkQueue::GetFrontTask() const {
  if (tasks_.empty())
    return nullptr;
  return &tasks_.front();
}

const TaskQueueImpl::Task* WorkQueue::GetBackTask() const {
  if (tasks_.empty())
    return nullptr;
  return &tasks_.back();
}

bool WorkQueue::BlockedByFence() const {
  if (!fence_)
    return false;

  // If the queue is empty then any future tasks will have a higher enqueue
  // order and will be blocked. The queue is also blocked if the head is past
  // the fence.
  return tasks_.empty() || tasks_.front().enqueue_order() >= fence_;
}

bool WorkQueue::GetFrontTaskEnqueueOrder(EnqueueOrder* enqueue_order) const {
  if (tasks_.empty() || BlockedByFence())
    return false;
  // Quick sanity check.
  DCHECK_LE(tasks_.front().enqueue_order(), tasks_.back().enqueue_order())
      << task_queue_->GetName() << " : " << work_queue_sets_->GetName() << " : "
      << name_;
  *enqueue_order = tasks_.front().enqueue_order();
  return true;
}

void WorkQueue::Push(TaskQueueImpl::Task task) {
  bool was_empty = tasks_.empty();
#ifndef NDEBUG
  DCHECK(task.enqueue_order_set());
#endif

  // Make sure the |enqueue_order()| is monotonically increasing.
  DCHECK(was_empty || tasks_.rbegin()->enqueue_order() < task.enqueue_order());

  // Amoritized O(1).
  tasks_.push_back(std::move(task));

  if (!was_empty)
    return;

  // If we hit the fence, pretend to WorkQueueSets that we're empty.
  if (work_queue_sets_ && !BlockedByFence())
    work_queue_sets_->OnTaskPushedToEmptyQueue(this);
}

void WorkQueue::PushNonNestableTaskToFront(TaskQueueImpl::Task task) {
  DCHECK(task.nestable == Nestable::kNonNestable);

  bool was_empty = tasks_.empty();
  bool was_blocked = BlockedByFence();
#ifndef NDEBUG
  DCHECK(task.enqueue_order_set());
#endif

  if (!was_empty) {
    // Make sure the |enqueue_order()| is monotonically increasing.
    DCHECK_LE(task.enqueue_order(), tasks_.front().enqueue_order())
        << task_queue_->GetName() << " : " << work_queue_sets_->GetName()
        << " : " << name_;
  }

  // Amoritized O(1).
  tasks_.push_front(std::move(task));

  if (!work_queue_sets_)
    return;

  // Pretend  to WorkQueueSets that nothing has changed if we're blocked.
  if (BlockedByFence())
    return;

  // Pushing task to front may unblock the fence.
  if (was_empty || was_blocked) {
    work_queue_sets_->OnTaskPushedToEmptyQueue(this);
  } else {
    work_queue_sets_->OnFrontTaskChanged(this);
  }
}

void WorkQueue::ReloadEmptyImmediateQueue() {
  DCHECK(tasks_.empty());

  task_queue_->ReloadEmptyImmediateQueue(&tasks_);
  if (tasks_.empty())
    return;

  // If we hit the fence, pretend to WorkQueueSets that we're empty.
  if (work_queue_sets_ && !BlockedByFence())
    work_queue_sets_->OnTaskPushedToEmptyQueue(this);
}

TaskQueueImpl::Task WorkQueue::TakeTaskFromWorkQueue() {
  DCHECK(work_queue_sets_);
  DCHECK(!tasks_.empty());

  TaskQueueImpl::Task pending_task = std::move(tasks_.front());
  tasks_.pop_front();
  // NB immediate tasks have a different pipeline to delayed ones.
  if (queue_type_ == QueueType::kImmediate && tasks_.empty()) {
    // Short-circuit the queue reload so that OnPopQueue does the right thing.
    task_queue_->ReloadEmptyImmediateQueue(&tasks_);
  }

  // OnPopQueue calls GetFrontTaskEnqueueOrder which checks BlockedByFence() so
  // we don't need to here.
  work_queue_sets_->OnPopQueue(this);
  task_queue_->TraceQueueSize();
  return pending_task;
}

bool WorkQueue::RemoveAllCanceledTasksFromFront() {
  DCHECK(work_queue_sets_);
  bool task_removed = false;
  while (!tasks_.empty() &&
         (!tasks_.front().task || tasks_.front().task.IsCancelled())) {
    tasks_.pop_front();
    task_removed = true;
  }
  if (task_removed) {
    // NB immediate tasks have a different pipeline to delayed ones.
    if (queue_type_ == QueueType::kImmediate && tasks_.empty()) {
      // Short-circuit the queue reload so that OnPopQueue does the right thing.
      task_queue_->ReloadEmptyImmediateQueue(&tasks_);
    }
    work_queue_sets_->OnPopQueue(this);
    task_queue_->TraceQueueSize();
  }
  return task_removed;
}

void WorkQueue::AssignToWorkQueueSets(WorkQueueSets* work_queue_sets) {
  work_queue_sets_ = work_queue_sets;
}

void WorkQueue::AssignSetIndex(size_t work_queue_set_index) {
  work_queue_set_index_ = work_queue_set_index;
}

bool WorkQueue::InsertFenceImpl(EnqueueOrder fence) {
  DCHECK_NE(fence, 0u);
  DCHECK(fence >= fence_ || fence == EnqueueOrder::blocking_fence());
  bool was_blocked_by_fence = BlockedByFence();
  fence_ = fence;
  return was_blocked_by_fence;
}

void WorkQueue::InsertFenceSilently(EnqueueOrder fence) {
  // Ensure that there is no fence present or a new one blocks queue completely.
  DCHECK(!fence_ || fence_ == EnqueueOrder::blocking_fence());
  InsertFenceImpl(fence);
}

bool WorkQueue::InsertFence(EnqueueOrder fence) {
  bool was_blocked_by_fence = InsertFenceImpl(fence);

  // Moving the fence forward may unblock some tasks.
  if (work_queue_sets_ && !tasks_.empty() && was_blocked_by_fence &&
      !BlockedByFence()) {
    work_queue_sets_->OnTaskPushedToEmptyQueue(this);
    return true;
  }
  // Fence insertion may have blocked all tasks in this work queue.
  if (BlockedByFence())
    work_queue_sets_->OnQueueBlocked(this);
  return false;
}

bool WorkQueue::RemoveFence() {
  bool was_blocked_by_fence = BlockedByFence();
  fence_ = EnqueueOrder::none();
  if (work_queue_sets_ && !tasks_.empty() && was_blocked_by_fence) {
    work_queue_sets_->OnTaskPushedToEmptyQueue(this);
    return true;
  }
  return false;
}

bool WorkQueue::ShouldRunBefore(const WorkQueue* other_queue) const {
  DCHECK(!tasks_.empty());
  DCHECK(!other_queue->tasks_.empty());
  EnqueueOrder enqueue_order;
  EnqueueOrder other_enqueue_order;
  bool have_task = GetFrontTaskEnqueueOrder(&enqueue_order);
  bool have_other_task =
      other_queue->GetFrontTaskEnqueueOrder(&other_enqueue_order);
  DCHECK(have_task);
  DCHECK(have_other_task);
  return enqueue_order < other_enqueue_order;
}

void WorkQueue::PopTaskForTesting() {
  if (tasks_.empty())
    return;
  tasks_.pop_front();
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
