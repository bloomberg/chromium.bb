// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/task_scheduler_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task_scheduler/sequence_sort_key.h"
#include "base/task_scheduler/task.h"
#include "base/time/time.h"

namespace base {
namespace internal {

// static
std::unique_ptr<TaskSchedulerImpl> TaskSchedulerImpl::Create() {
  std::unique_ptr<TaskSchedulerImpl> scheduler(new TaskSchedulerImpl);
  scheduler->Initialize();
  return scheduler;
}

TaskSchedulerImpl::~TaskSchedulerImpl() {
#if DCHECK_IS_ON()
  DCHECK(join_for_testing_returned_.IsSignaled());
#endif
}

void TaskSchedulerImpl::PostTaskWithTraits(
    const tracked_objects::Location& from_here,
    const TaskTraits& traits,
    const Closure& task) {
  // Post |task| as part of a one-off single-task Sequence.
  GetThreadPoolForTraits(traits)->PostTaskWithSequence(
      WrapUnique(new Task(from_here, task, traits, TimeDelta())),
      make_scoped_refptr(new Sequence), nullptr);
}

scoped_refptr<TaskRunner> TaskSchedulerImpl::CreateTaskRunnerWithTraits(
    const TaskTraits& traits,
    ExecutionMode execution_mode) {
  return GetThreadPoolForTraits(traits)->CreateTaskRunnerWithTraits(
      traits, execution_mode);
}

void TaskSchedulerImpl::Shutdown() {
  // TODO(fdoray): Increase the priority of BACKGROUND tasks blocking shutdown.
  task_tracker_.Shutdown();
}

void TaskSchedulerImpl::JoinForTesting() {
#if DCHECK_IS_ON()
  DCHECK(!join_for_testing_returned_.IsSignaled());
#endif
  background_thread_pool_->JoinForTesting();
  background_file_io_thread_pool_->JoinForTesting();
  normal_thread_pool_->JoinForTesting();
  normal_file_io_thread_pool_->JoinForTesting();
#if DCHECK_IS_ON()
  join_for_testing_returned_.Signal();
#endif
}

TaskSchedulerImpl::TaskSchedulerImpl()
    // TODO(robliao): Wake up the service thread instead of calling DoNothing()
    // when the delayed run time changes.
    : delayed_task_manager_(Bind(&DoNothing))
#if DCHECK_IS_ON()
          ,
      join_for_testing_returned_(true, false)
#endif
{
}

void TaskSchedulerImpl::Initialize() {
  using IORestriction = SchedulerThreadPoolImpl::IORestriction;

  const SchedulerThreadPoolImpl::ReEnqueueSequenceCallback
      re_enqueue_sequence_callback =
          Bind(&TaskSchedulerImpl::ReEnqueueSequenceCallback, Unretained(this));

  // TODO(fdoray): Derive the number of threads per pool from hardware
  // characteristics rather than using hard-coded constants.

  // Passing pointers to objects owned by |this| to
  // SchedulerThreadPoolImpl::Create() is safe because a TaskSchedulerImpl can't
  // be deleted before all its thread pools have been joined.
  background_thread_pool_ = SchedulerThreadPoolImpl::Create(
      ThreadPriority::BACKGROUND, 1U, IORestriction::DISALLOWED,
      re_enqueue_sequence_callback, &task_tracker_, &delayed_task_manager_);
  CHECK(background_thread_pool_);

  background_file_io_thread_pool_ = SchedulerThreadPoolImpl::Create(
      ThreadPriority::BACKGROUND, 1U, IORestriction::ALLOWED,
      re_enqueue_sequence_callback, &task_tracker_, &delayed_task_manager_);
  CHECK(background_file_io_thread_pool_);

  normal_thread_pool_ = SchedulerThreadPoolImpl::Create(
      ThreadPriority::NORMAL, 4U, IORestriction::DISALLOWED,
      re_enqueue_sequence_callback, &task_tracker_, &delayed_task_manager_);
  CHECK(normal_thread_pool_);

  normal_file_io_thread_pool_ = SchedulerThreadPoolImpl::Create(
      ThreadPriority::NORMAL, 12U, IORestriction::ALLOWED,
      re_enqueue_sequence_callback, &task_tracker_, &delayed_task_manager_);
  CHECK(normal_file_io_thread_pool_);
}

SchedulerThreadPool* TaskSchedulerImpl::GetThreadPoolForTraits(
    const TaskTraits& traits) {
  if (traits.with_file_io()) {
    if (traits.priority() == TaskPriority::BACKGROUND)
      return background_file_io_thread_pool_.get();
    return normal_file_io_thread_pool_.get();
  }

  if (traits.priority() == TaskPriority::BACKGROUND)
    return background_thread_pool_.get();
  return normal_thread_pool_.get();
}

void TaskSchedulerImpl::ReEnqueueSequenceCallback(
    scoped_refptr<Sequence> sequence) {
  DCHECK(sequence);

  const SequenceSortKey sort_key = sequence->GetSortKey();
  TaskTraits traits(sequence->PeekTask()->traits);

  // Update the priority of |traits| so that the next task in |sequence| runs
  // with the highest priority in |sequence| as opposed to the next task's
  // specific priority.
  traits.WithPriority(sort_key.priority());

  GetThreadPoolForTraits(traits)->ReEnqueueSequence(std::move(sequence),
                                                    sort_key);
}

}  // namespace internal
}  // namespace base
