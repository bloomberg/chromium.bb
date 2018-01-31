// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/deferred_sequenced_task_runner.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"

namespace base {

DeferredSequencedTaskRunner::DeferredTask::DeferredTask()
    : is_non_nestable(false) {
}

DeferredSequencedTaskRunner::DeferredTask::DeferredTask(DeferredTask&& other) =
    default;

DeferredSequencedTaskRunner::DeferredTask::~DeferredTask() = default;

DeferredSequencedTaskRunner::DeferredTask&
DeferredSequencedTaskRunner::DeferredTask::operator=(DeferredTask&& other) =
    default;

DeferredSequencedTaskRunner::DeferredSequencedTaskRunner(
    scoped_refptr<SequencedTaskRunner> target_task_runner)
    : DeferredSequencedTaskRunner(std::move(target_task_runner), false) {}

DeferredSequencedTaskRunner::DeferredSequencedTaskRunner(
    bool does_target_task_runner_run_tasks_in_sequence)
    : DeferredSequencedTaskRunner(
          nullptr,
          does_target_task_runner_run_tasks_in_sequence) {}

bool DeferredSequencedTaskRunner::PostDelayedTask(const Location& from_here,
                                                  OnceClosure task,
                                                  TimeDelta delay) {
  AutoLock lock(lock_);
  if (started_) {
    DCHECK(deferred_tasks_queue_.empty());
    return target_task_runner_->PostDelayedTask(from_here, std::move(task),
                                                delay);
  }

  QueueDeferredTask(from_here, std::move(task), delay,
                    false /* is_non_nestable */);
  return true;
}

bool DeferredSequencedTaskRunner::RunsTasksInCurrentSequence() const {
  AutoLock lock(lock_);
  return target_task_runner_ ? target_task_runner_->RunsTasksInCurrentSequence()
                             : does_target_task_runner_run_tasks_in_sequence_;
}

bool DeferredSequencedTaskRunner::PostNonNestableDelayedTask(
    const Location& from_here,
    OnceClosure task,
    TimeDelta delay) {
  AutoLock lock(lock_);
  if (started_) {
    DCHECK(deferred_tasks_queue_.empty());
    return target_task_runner_->PostNonNestableDelayedTask(
        from_here, std::move(task), delay);
  }
  QueueDeferredTask(from_here, std::move(task), delay,
                    true /* is_non_nestable */);
  return true;
}

DeferredSequencedTaskRunner::DeferredSequencedTaskRunner(
    scoped_refptr<SequencedTaskRunner> target_task_runner,
    bool does_target_task_runner_run_tasks_in_sequence)
    : does_target_task_runner_run_tasks_in_sequence_(
          does_target_task_runner_run_tasks_in_sequence),
      started_(false),
      target_task_runner_(std::move(target_task_runner)) {}

DeferredSequencedTaskRunner::~DeferredSequencedTaskRunner() = default;

void DeferredSequencedTaskRunner::QueueDeferredTask(const Location& from_here,
                                                    OnceClosure task,
                                                    TimeDelta delay,
                                                    bool is_non_nestable) {
  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task);

  DeferredTask deferred_task;
  deferred_task.posted_from = from_here;
  deferred_task.task = std::move(task);
  deferred_task.delay = delay;
  deferred_task.is_non_nestable = is_non_nestable;
  deferred_tasks_queue_.push_back(std::move(deferred_task));
}

void DeferredSequencedTaskRunner::Start(
    scoped_refptr<SequencedTaskRunner> target_task_runner) {
  AutoLock lock(lock_);
  DCHECK(!started_);
  started_ = true;
  if (target_task_runner) {
    DCHECK(!target_task_runner_);
    target_task_runner_ = std::move(target_task_runner);
  }
  DCHECK(target_task_runner_);
  for (std::vector<DeferredTask>::iterator i = deferred_tasks_queue_.begin();
      i != deferred_tasks_queue_.end();
      ++i) {
    DeferredTask& task = *i;
    if (task.is_non_nestable) {
      target_task_runner_->PostNonNestableDelayedTask(
          task.posted_from, std::move(task.task), task.delay);
    } else {
      target_task_runner_->PostDelayedTask(task.posted_from,
                                           std::move(task.task), task.delay);
    }
  }
  deferred_tasks_queue_.clear();
}

}  // namespace base
