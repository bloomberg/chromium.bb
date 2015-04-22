// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/worker_scheduler_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/child/nestable_single_thread_task_runner.h"

namespace scheduler {

WorkerSchedulerImpl::WorkerSchedulerImpl(
    scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner)
    : helper_(main_task_runner,
              this,
              "worker.scheduler",
              TRACE_DISABLED_BY_DEFAULT("worker.scheduler"),
              "WorkerSchedulerIdlePeriod",
              SchedulerHelper::TASK_QUEUE_COUNT,
              base::TimeDelta::FromMilliseconds(300)) {
  initialized_ = false;
  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"), "WorkerScheduler", this);
}

WorkerSchedulerImpl::~WorkerSchedulerImpl() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"), "WorkerScheduler", this);
}

void WorkerSchedulerImpl::Init() {
  initialized_ = true;
  helper_.EnableLongIdlePeriod();
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerSchedulerImpl::DefaultTaskRunner() {
  DCHECK(initialized_);
  return helper_.DefaultTaskRunner();
}

scoped_refptr<SingleThreadIdleTaskRunner>
WorkerSchedulerImpl::IdleTaskRunner() {
  DCHECK(initialized_);
  return helper_.IdleTaskRunner();
}

bool WorkerSchedulerImpl::CanExceedIdleDeadlineIfRequired() const {
  DCHECK(initialized_);
  return helper_.CanExceedIdleDeadlineIfRequired();
}

bool WorkerSchedulerImpl::ShouldYieldForHighPriorityWork() {
  // We don't consider any work as being high priority on workers.
  return false;
}

void WorkerSchedulerImpl::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK(initialized_);
  helper_.AddTaskObserver(task_observer);
}

void WorkerSchedulerImpl::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK(initialized_);
  helper_.RemoveTaskObserver(task_observer);
}

void WorkerSchedulerImpl::Shutdown() {
  DCHECK(initialized_);
  helper_.Shutdown();
}

SchedulerHelper* WorkerSchedulerImpl::GetSchedulerHelperForTesting() {
  return &helper_;
}

bool WorkerSchedulerImpl::CanEnterLongIdlePeriod(base::TimeTicks,
                                                 base::TimeDelta*) {
  return true;
}

base::TimeTicks WorkerSchedulerImpl::CurrentIdleTaskDeadlineForTesting() const {
  base::TimeTicks deadline;
  helper_.CurrentIdleTaskDeadlineCallback(&deadline);
  return deadline;
}

}  // namespace scheduler
