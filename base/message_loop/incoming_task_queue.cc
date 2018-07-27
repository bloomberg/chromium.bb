// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/incoming_task_queue.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"

namespace base {
namespace internal {

IncomingTaskQueue::IncomingTaskQueue() = default;

IncomingTaskQueue::~IncomingTaskQueue() = default;

void IncomingTaskQueue::ReportMetricsOnIdle() const {
  UMA_HISTOGRAM_COUNTS_1M(
      "MessageLoop.DelayedTaskQueueForUI.PendingTasksCountOnIdle",
      delayed_tasks_.Size());
}

IncomingTaskQueue::DelayedQueue::DelayedQueue() {
  // The constructing sequence is not necessarily the running sequence, e.g. in
  // the case of a MessageLoop created unbound.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

IncomingTaskQueue::DelayedQueue::~DelayedQueue() = default;

void IncomingTaskQueue::DelayedQueue::Push(PendingTask pending_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pending_task.is_high_res)
    ++pending_high_res_tasks_;

  queue_.push(std::move(pending_task));
}

const PendingTask& IncomingTaskQueue::DelayedQueue::Peek() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!queue_.empty());
  return queue_.top();
}

PendingTask IncomingTaskQueue::DelayedQueue::Pop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!queue_.empty());
  PendingTask delayed_task = std::move(const_cast<PendingTask&>(queue_.top()));
  queue_.pop();

  if (delayed_task.is_high_res)
    --pending_high_res_tasks_;
  DCHECK_GE(pending_high_res_tasks_, 0);

  return delayed_task;
}

bool IncomingTaskQueue::DelayedQueue::HasTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(robliao): The other queues don't check for IsCancelled(). Should they?
  while (!queue_.empty() && Peek().task.IsCancelled())
    Pop();

  return !queue_.empty();
}

void IncomingTaskQueue::DelayedQueue::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (!queue_.empty())
    Pop();
}

size_t IncomingTaskQueue::DelayedQueue::Size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return queue_.size();
}

IncomingTaskQueue::DeferredQueue::DeferredQueue() {
  // The constructing sequence is not necessarily the running sequence, e.g. in
  // the case of a MessageLoop created unbound.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

IncomingTaskQueue::DeferredQueue::~DeferredQueue() = default;

void IncomingTaskQueue::DeferredQueue::Push(PendingTask pending_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  queue_.push(std::move(pending_task));
}

const PendingTask& IncomingTaskQueue::DeferredQueue::Peek() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!queue_.empty());
  return queue_.front();
}

PendingTask IncomingTaskQueue::DeferredQueue::Pop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!queue_.empty());
  PendingTask deferred_task = std::move(queue_.front());
  queue_.pop();
  return deferred_task;
}

bool IncomingTaskQueue::DeferredQueue::HasTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !queue_.empty();
}

void IncomingTaskQueue::DeferredQueue::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (!queue_.empty())
    Pop();
}

}  // namespace internal
}  // namespace base
