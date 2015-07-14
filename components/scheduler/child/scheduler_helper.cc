// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/scheduler_helper.h"

#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/child/nestable_single_thread_task_runner.h"

namespace scheduler {

SchedulerHelper::SchedulerHelper(
    scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner,
    const char* tracing_category,
    const char* disabled_by_default_tracing_category,
    const char* disabled_by_default_verbose_tracing_category,
    size_t total_task_queue_count)
    : task_queue_selector_(new PrioritizingTaskQueueSelector()),
      task_queue_manager_(
          new TaskQueueManager(total_task_queue_count,
                               main_task_runner,
                               task_queue_selector_.get(),
                               disabled_by_default_tracing_category,
                               disabled_by_default_verbose_tracing_category)),
      quiescence_monitored_task_queue_mask_(
          ((1ull << total_task_queue_count) - 1ull) &
          ~(1ull << QueueId::CONTROL_TASK_QUEUE) &
          ~(1ull << QueueId::CONTROL_TASK_AFTER_WAKEUP_QUEUE)),
      control_task_runner_(
          task_queue_manager_->TaskRunnerForQueue(QueueId::CONTROL_TASK_QUEUE)),
      control_after_wakeup_task_runner_(task_queue_manager_->TaskRunnerForQueue(
          QueueId::CONTROL_TASK_AFTER_WAKEUP_QUEUE)),
      default_task_runner_(
          task_queue_manager_->TaskRunnerForQueue(QueueId::DEFAULT_TASK_QUEUE)),
      time_source_(new base::DefaultTickClock),
      tracing_category_(tracing_category),
      disabled_by_default_tracing_category_(
          disabled_by_default_tracing_category) {
  DCHECK_GE(total_task_queue_count,
            static_cast<size_t>(QueueId::TASK_QUEUE_COUNT));
  task_queue_selector_->SetQueuePriority(
      QueueId::CONTROL_TASK_QUEUE,
      PrioritizingTaskQueueSelector::CONTROL_PRIORITY);
  task_queue_manager_->SetWakeupPolicy(
      QueueId::CONTROL_TASK_QUEUE,
      TaskQueueManager::WakeupPolicy::DONT_WAKE_OTHER_QUEUES);

  task_queue_selector_->SetQueuePriority(
      QueueId::CONTROL_TASK_AFTER_WAKEUP_QUEUE,
      PrioritizingTaskQueueSelector::CONTROL_PRIORITY);
  task_queue_manager_->SetPumpPolicy(
      QueueId::CONTROL_TASK_AFTER_WAKEUP_QUEUE,
      TaskQueueManager::PumpPolicy::AFTER_WAKEUP);
  task_queue_manager_->SetWakeupPolicy(
      QueueId::CONTROL_TASK_AFTER_WAKEUP_QUEUE,
      TaskQueueManager::WakeupPolicy::DONT_WAKE_OTHER_QUEUES);

  for (size_t i = 0; i < TASK_QUEUE_COUNT; i++) {
    task_queue_manager_->SetQueueName(
        i, TaskQueueIdToString(static_cast<QueueId>(i)));
  }

  // TODO(skyostil): Increase this to 4 (crbug.com/444764).
  task_queue_manager_->SetWorkBatchSize(1);
}

SchedulerHelper::~SchedulerHelper() {
}

void SchedulerHelper::Shutdown() {
  CheckOnValidThread();
  task_queue_manager_.reset();
}

scoped_refptr<base::SingleThreadTaskRunner>
SchedulerHelper::DefaultTaskRunner() {
  CheckOnValidThread();
  return default_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
SchedulerHelper::ControlTaskRunner() {
  return control_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
SchedulerHelper::ControlAfterWakeUpTaskRunner() {
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

scoped_refptr<base::SingleThreadTaskRunner> SchedulerHelper::TaskRunnerForQueue(
    size_t queue_index) const {
  CheckOnValidThread();
  return task_queue_manager_->TaskRunnerForQueue(queue_index);
}

base::TimeTicks SchedulerHelper::NextPendingDelayedTaskRunTime() const {
  CheckOnValidThread();
  return task_queue_manager_->NextPendingDelayedTaskRunTime();
}

void SchedulerHelper::SetQueueName(size_t queue_index, const char* name) {
  CheckOnValidThread();
  task_queue_manager_->SetQueueName(queue_index, name);
}

bool SchedulerHelper::IsQueueEmpty(size_t queue_index) const {
  CheckOnValidThread();
  return task_queue_manager_->IsQueueEmpty(queue_index);
}

TaskQueueManager::QueueState SchedulerHelper::GetQueueState(size_t queue_index)
    const {
  CheckOnValidThread();
  return task_queue_manager_->GetQueueState(queue_index);
}

void SchedulerHelper::SetQueuePriority(
    size_t queue_index,
    PrioritizingTaskQueueSelector::QueuePriority priority) {
  CheckOnValidThread();
  return task_queue_selector_->SetQueuePriority(queue_index, priority);
}

void SchedulerHelper::EnableQueue(
    size_t queue_index,
    PrioritizingTaskQueueSelector::QueuePriority priority) {
  CheckOnValidThread();
  task_queue_selector_->EnableQueue(queue_index, priority);
}

void SchedulerHelper::DisableQueue(size_t queue_index) {
  CheckOnValidThread();
  task_queue_selector_->DisableQueue(queue_index);
}

bool SchedulerHelper::IsQueueEnabled(size_t queue_index) const {
  CheckOnValidThread();
  return task_queue_selector_->IsQueueEnabled(queue_index);
}

void SchedulerHelper::SetPumpPolicy(size_t queue_index,
                                    TaskQueueManager::PumpPolicy pump_policy) {
  CheckOnValidThread();
  return task_queue_manager_->SetPumpPolicy(queue_index, pump_policy);
}

void SchedulerHelper::SetWakeupPolicy(
    size_t queue_index,
    TaskQueueManager::WakeupPolicy wakeup_policy) {
  CheckOnValidThread();
  return task_queue_manager_->SetWakeupPolicy(queue_index, wakeup_policy);
}

void SchedulerHelper::PumpQueue(size_t queue_index) {
  CheckOnValidThread();
  return task_queue_manager_->PumpQueue(queue_index);
}

uint64 SchedulerHelper::GetQuiescenceMonitoredTaskQueueMask() const {
  CheckOnValidThread();
  return quiescence_monitored_task_queue_mask_;
}

uint64 SchedulerHelper::GetAndClearTaskWasRunOnQueueBitmap() {
  CheckOnValidThread();
  return task_queue_manager_->GetAndClearTaskWasRunOnQueueBitmap();
}

// static
const char* SchedulerHelper::TaskQueueIdToString(QueueId queue_id) {
  switch (queue_id) {
    case DEFAULT_TASK_QUEUE:
      return "default_tq";
    case CONTROL_TASK_QUEUE:
      return "control_tq";
    case CONTROL_TASK_AFTER_WAKEUP_QUEUE:
      return "control_after_wakeup_tq";
    default:
      NOTREACHED();
      return nullptr;
  }
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
