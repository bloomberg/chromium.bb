// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_queue_proxy.h"

#include "base/task/sequence_manager/associated_thread_id.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue_impl.h"

namespace base {
namespace sequence_manager {
namespace internal {

TaskQueueProxy::TaskQueueProxy(
    TaskQueueImpl* task_queue_impl,
    scoped_refptr<AssociatedThreadId> associated_thread)
    : task_queue_impl_(task_queue_impl),
      associated_thread_(std::move(associated_thread)) {}

TaskQueueProxy::~TaskQueueProxy() = default;

bool TaskQueueProxy::PostTask(TaskQueue::PostedTask task) const {
  Optional<MoveableAutoLock> lock(AcquireLockIfNeeded());
  if (!task_queue_impl_)
    return false;

  TaskQueueImpl::PostTaskResult result(
      task_queue_impl_->PostDelayedTask(std::move(task)));
  // If posting task was unsuccessful then |result| will contain
  // the original task which should be destructed outside of the lock
  // because new tasks may be posted in the destrictor.
  lock = nullopt;
  return result.success;
}

bool TaskQueueProxy::RunsTasksInCurrentSequence() const {
  return associated_thread_->thread_id == PlatformThread::CurrentId();
}

Optional<MoveableAutoLock> TaskQueueProxy::AcquireLockIfNeeded() const {
  if (RunsTasksInCurrentSequence())
    return nullopt;
  return MoveableAutoLock(lock_);
}

void TaskQueueProxy::DetachFromTaskQueueImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  AutoLock lock(lock_);
  // Main thread is the only thread where |task_queue_impl_| is being read
  // without a lock, which is fine because this function is main thread only.
  task_queue_impl_ = nullptr;
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
