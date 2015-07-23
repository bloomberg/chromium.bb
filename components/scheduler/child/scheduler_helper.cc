// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/scheduler_helper.h"

#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/child/scheduler_task_runner_delegate.h"
#include "components/scheduler/child/task_queue_impl.h"

namespace scheduler {

SchedulerHelper::SchedulerHelper(
    scoped_refptr<SchedulerTaskRunnerDelegate> main_task_runner,
    const char* tracing_category,
    const char* disabled_by_default_tracing_category,
    const char* disabled_by_default_verbose_tracing_category)
    : main_task_runner_(main_task_runner),
      task_queue_manager_(
          new TaskQueueManager(main_task_runner,
                               disabled_by_default_tracing_category,
                               disabled_by_default_verbose_tracing_category)),
      control_task_runner_(NewTaskQueue(
          TaskQueue::Spec("control_tq")
              .SetWakeupPolicy(
                  TaskQueue::WakeupPolicy::DONT_WAKE_OTHER_QUEUES))),
      control_after_wakeup_task_runner_(NewTaskQueue(
          TaskQueue::Spec("control_after_wakeup_tq")
              .SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP)
              .SetWakeupPolicy(
                  TaskQueue::WakeupPolicy::DONT_WAKE_OTHER_QUEUES))),
      default_task_runner_(NewTaskQueue(TaskQueue::Spec("default_tq")
                                            .SetShouldMonitorQuiescence(true))),
      time_source_(new base::DefaultTickClock),
      tracing_category_(tracing_category),
      disabled_by_default_tracing_category_(
          disabled_by_default_tracing_category) {
  control_task_runner_->SetQueuePriority(TaskQueue::CONTROL_PRIORITY);
  control_after_wakeup_task_runner_->SetQueuePriority(
      TaskQueue::CONTROL_PRIORITY);

  // TODO(skyostil): Increase this to 4 (crbug.com/444764).
  task_queue_manager_->SetWorkBatchSize(1);

  main_task_runner_->SetDefaultTaskRunner(default_task_runner_.get());
}

SchedulerHelper::~SchedulerHelper() {
  Shutdown();
}

void SchedulerHelper::Shutdown() {
  CheckOnValidThread();
  task_queue_manager_.reset();
  main_task_runner_->RestoreDefaultTaskRunner();
}

scoped_refptr<TaskQueue> SchedulerHelper::NewTaskQueue(
    const TaskQueue::Spec& spec) {
  return task_queue_manager_->NewTaskQueue(spec);
}

scoped_refptr<TaskQueue> SchedulerHelper::DefaultTaskRunner() {
  CheckOnValidThread();
  return default_task_runner_;
}

scoped_refptr<TaskQueue> SchedulerHelper::ControlTaskRunner() {
  return control_task_runner_;
}

scoped_refptr<TaskQueue> SchedulerHelper::ControlAfterWakeUpTaskRunner() {
  return control_after_wakeup_task_runner_;
}

void SchedulerHelper::SetTimeSourceForTesting(
    scoped_ptr<base::TickClock> time_source) {
  CheckOnValidThread();
  time_source_ = time_source.Pass();
}

void SchedulerHelper::SetWorkBatchSizeForTesting(size_t work_batch_size) {
  CheckOnValidThread();
  task_queue_manager_->SetWorkBatchSize(work_batch_size);
}

TaskQueueManager* SchedulerHelper::GetTaskQueueManagerForTesting() {
  CheckOnValidThread();
  return task_queue_manager_.get();
}

base::TimeTicks SchedulerHelper::Now() const {
  return time_source_->NowTicks();
}

base::TimeTicks SchedulerHelper::NextPendingDelayedTaskRunTime() const {
  CheckOnValidThread();
  return task_queue_manager_->NextPendingDelayedTaskRunTime();
}

bool SchedulerHelper::GetAndClearSystemIsQuiescentBit() {
  CheckOnValidThread();
  return task_queue_manager_->GetAndClearSystemIsQuiescentBit();
}

void SchedulerHelper::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  CheckOnValidThread();
  if (task_queue_manager_)
    task_queue_manager_->AddTaskObserver(task_observer);
}

void SchedulerHelper::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  CheckOnValidThread();
  if (task_queue_manager_)
    task_queue_manager_->RemoveTaskObserver(task_observer);
}

}  // namespace scheduler
