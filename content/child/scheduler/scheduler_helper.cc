// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/scheduler/scheduler_helper.h"

#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "content/child/scheduler/nestable_single_thread_task_runner.h"
#include "content/child/scheduler/prioritizing_task_queue_selector.h"

namespace content {

SchedulerHelper::SchedulerHelper(
    scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner,
    SchedulerHelperDelegate* scheduler_helper_delegate,
    const char* tracing_category,
    const char* disabled_by_default_tracing_category,
    size_t total_task_queue_count)
    : task_queue_selector_(new PrioritizingTaskQueueSelector()),
      task_queue_manager_(
          new TaskQueueManager(total_task_queue_count,
                               main_task_runner,
                               task_queue_selector_.get(),
                               disabled_by_default_tracing_category)),
      idle_period_state_(IdlePeriodState::NOT_IN_IDLE_PERIOD),
      scheduler_helper_delegate_(scheduler_helper_delegate),
      control_task_runner_(
          task_queue_manager_->TaskRunnerForQueue(QueueId::CONTROL_TASK_QUEUE)),
      control_task_after_wakeup_runner_(task_queue_manager_->TaskRunnerForQueue(
          QueueId::CONTROL_TASK_AFTER_WAKEUP_QUEUE)),
      default_task_runner_(
          task_queue_manager_->TaskRunnerForQueue(QueueId::DEFAULT_TASK_QUEUE)),
      tracing_category_(tracing_category),
      disabled_by_default_tracing_category_(
          disabled_by_default_tracing_category),
      weak_factory_(this) {
  DCHECK_GE(total_task_queue_count,
            static_cast<size_t>(QueueId::TASK_QUEUE_COUNT));
  weak_scheduler_ptr_ = weak_factory_.GetWeakPtr();
  end_idle_period_closure_.Reset(
      base::Bind(&SchedulerHelper::EndIdlePeriod, weak_scheduler_ptr_));
  initiate_next_long_idle_period_closure_.Reset(base::Bind(
      &SchedulerHelper::InitiateLongIdlePeriod, weak_scheduler_ptr_));
  initiate_next_long_idle_period_after_wakeup_closure_.Reset(
      base::Bind(&SchedulerHelper::InitiateLongIdlePeriodAfterWakeup,
                 weak_scheduler_ptr_));

  idle_task_runner_ = make_scoped_refptr(new SingleThreadIdleTaskRunner(
      task_queue_manager_->TaskRunnerForQueue(QueueId::IDLE_TASK_QUEUE),
      control_task_after_wakeup_runner_,
      base::Bind(&SchedulerHelper::CurrentIdleTaskDeadlineCallback,
                 weak_scheduler_ptr_),
      tracing_category));

  task_queue_selector_->SetQueuePriority(
      QueueId::CONTROL_TASK_QUEUE,
      PrioritizingTaskQueueSelector::CONTROL_PRIORITY);

  task_queue_selector_->SetQueuePriority(
      QueueId::CONTROL_TASK_AFTER_WAKEUP_QUEUE,
      PrioritizingTaskQueueSelector::CONTROL_PRIORITY);
  task_queue_manager_->SetPumpPolicy(
      QueueId::CONTROL_TASK_AFTER_WAKEUP_QUEUE,
      TaskQueueManager::PumpPolicy::AFTER_WAKEUP);

  task_queue_selector_->DisableQueue(QueueId::IDLE_TASK_QUEUE);
  task_queue_manager_->SetPumpPolicy(QueueId::IDLE_TASK_QUEUE,
                                     TaskQueueManager::PumpPolicy::MANUAL);

  for (size_t i = 0; i < TASK_QUEUE_COUNT; i++) {
    task_queue_manager_->SetQueueName(
        i, TaskQueueIdToString(static_cast<QueueId>(i)));
  }

  // TODO(skyostil): Increase this to 4 (crbug.com/444764).
  task_queue_manager_->SetWorkBatchSize(1);
}

SchedulerHelper::~SchedulerHelper() {
}

SchedulerHelper::SchedulerHelperDelegate::SchedulerHelperDelegate() {
}

SchedulerHelper::SchedulerHelperDelegate::~SchedulerHelperDelegate() {
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

scoped_refptr<SingleThreadIdleTaskRunner> SchedulerHelper::IdleTaskRunner() {
  CheckOnValidThread();
  return idle_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
SchedulerHelper::ControlTaskRunner() {
  return control_task_runner_;
}

void SchedulerHelper::CurrentIdleTaskDeadlineCallback(
    base::TimeTicks* deadline_out) const {
  CheckOnValidThread();
  *deadline_out = idle_period_deadline_;
}

SchedulerHelper::IdlePeriodState SchedulerHelper::ComputeNewLongIdlePeriodState(
    const base::TimeTicks now,
    base::TimeDelta* next_long_idle_period_delay_out) {
  CheckOnValidThread();

  if (!scheduler_helper_delegate_->CanEnterLongIdlePeriod(
          now, next_long_idle_period_delay_out)) {
    return IdlePeriodState::NOT_IN_IDLE_PERIOD;
  }

  base::TimeTicks next_pending_delayed_task =
      task_queue_manager_->NextPendingDelayedTaskRunTime();
  base::TimeDelta max_long_idle_period_duration =
      base::TimeDelta::FromMilliseconds(kMaximumIdlePeriodMillis);
  base::TimeDelta long_idle_period_duration;
  if (next_pending_delayed_task.is_null()) {
    long_idle_period_duration = max_long_idle_period_duration;
  } else {
    // Limit the idle period duration to be before the next pending task.
    long_idle_period_duration = std::min(next_pending_delayed_task - now,
                                         max_long_idle_period_duration);
  }

  if (long_idle_period_duration > base::TimeDelta()) {
    *next_long_idle_period_delay_out = long_idle_period_duration;
    return long_idle_period_duration == max_long_idle_period_duration
               ? IdlePeriodState::IN_LONG_IDLE_PERIOD_WITH_MAX_DEADLINE
               : IdlePeriodState::IN_LONG_IDLE_PERIOD;
  } else {
    // If we can't start the idle period yet then try again after wakeup.
    *next_long_idle_period_delay_out = base::TimeDelta::FromMilliseconds(
        kRetryInitiateLongIdlePeriodDelayMillis);
    return IdlePeriodState::NOT_IN_IDLE_PERIOD;
  }
}

void SchedulerHelper::InitiateLongIdlePeriod() {
  TRACE_EVENT0(disabled_by_default_tracing_category_, "InitiateLongIdlePeriod");
  CheckOnValidThread();

  // End any previous idle period.
  EndIdlePeriod();

  base::TimeTicks now(Now());
  base::TimeDelta next_long_idle_period_delay;
  IdlePeriodState new_idle_period_state =
      ComputeNewLongIdlePeriodState(now, &next_long_idle_period_delay);
  if (IsInIdlePeriod(new_idle_period_state)) {
    StartIdlePeriod(new_idle_period_state, now,
                    now + next_long_idle_period_delay,
                    false);
  }

  if (task_queue_manager_->IsQueueEmpty(QueueId::IDLE_TASK_QUEUE)) {
    // If there are no current idle tasks then post the call to initiate the
    // next idle for execution after wakeup (at which point after-wakeup idle
    // tasks might be eligible to run or more idle tasks posted).
    control_task_after_wakeup_runner_->PostDelayedTask(
        FROM_HERE,
        initiate_next_long_idle_period_after_wakeup_closure_.callback(),
        next_long_idle_period_delay);
  } else {
    // Otherwise post on the normal control task queue.
    control_task_runner_->PostDelayedTask(
        FROM_HERE, initiate_next_long_idle_period_closure_.callback(),
        next_long_idle_period_delay);
  }
}

void SchedulerHelper::InitiateLongIdlePeriodAfterWakeup() {
  TRACE_EVENT0(disabled_by_default_tracing_category_,
               "InitiateLongIdlePeriodAfterWakeup");
  CheckOnValidThread();

  if (IsInIdlePeriod(idle_period_state_)) {
    // Since we were asleep until now, end the async idle period trace event at
    // the time when it would have ended were we awake.
    TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP0(
        tracing_category_, "RendererSchedulerIdlePeriod", this,
        std::min(idle_period_deadline_, Now()).ToInternalValue());
    idle_period_state_ = IdlePeriodState::ENDING_LONG_IDLE_PERIOD;
    EndIdlePeriod();
  }

  // Post a task to initiate the next long idle period rather than calling it
  // directly to allow all pending PostIdleTaskAfterWakeup tasks to get enqueued
  // on the idle task queue before the next idle period starts so they are
  // eligible to be run during the new idle period.
  control_task_runner_->PostTask(
      FROM_HERE, initiate_next_long_idle_period_closure_.callback());
}

void SchedulerHelper::StartIdlePeriod(IdlePeriodState new_state,
                                      base::TimeTicks now,
                                      base::TimeTicks idle_period_deadline,
                                      bool post_end_idle_period) {
  DCHECK_GT(idle_period_deadline, now);
  TRACE_EVENT_ASYNC_BEGIN0(tracing_category_, "RendererSchedulerIdlePeriod",
                           this);
  CheckOnValidThread();
  DCHECK(IsInIdlePeriod(new_state));

  task_queue_selector_->EnableQueue(
      QueueId::IDLE_TASK_QUEUE,
      PrioritizingTaskQueueSelector::BEST_EFFORT_PRIORITY);
  task_queue_manager_->PumpQueue(QueueId::IDLE_TASK_QUEUE);
  idle_period_state_ = new_state;

  idle_period_deadline_ = idle_period_deadline;
  if (post_end_idle_period) {
    control_task_runner_->PostDelayedTask(
        FROM_HERE,
        end_idle_period_closure_.callback(),
        idle_period_deadline_ - now);
  }
}

void SchedulerHelper::EndIdlePeriod() {
  CheckOnValidThread();

  end_idle_period_closure_.Cancel();
  initiate_next_long_idle_period_closure_.Cancel();
  initiate_next_long_idle_period_after_wakeup_closure_.Cancel();

  // If we weren't already within an idle period then early-out.
  if (!IsInIdlePeriod(idle_period_state_))
    return;

  // If we are in the ENDING_LONG_IDLE_PERIOD state we have already logged the
  // trace event.
  if (idle_period_state_ != IdlePeriodState::ENDING_LONG_IDLE_PERIOD) {
    bool is_tracing;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(tracing_category_, &is_tracing);
    if (is_tracing && !idle_period_deadline_.is_null() &&
        base::TimeTicks::Now() > idle_period_deadline_) {
      TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0(
          tracing_category_, "RendererSchedulerIdlePeriod", this,
          "DeadlineOverrun", idle_period_deadline_.ToInternalValue());
    }
    TRACE_EVENT_ASYNC_END0(tracing_category_, "RendererSchedulerIdlePeriod",
                           this);
  }

  task_queue_selector_->DisableQueue(QueueId::IDLE_TASK_QUEUE);
  idle_period_state_ = IdlePeriodState::NOT_IN_IDLE_PERIOD;
  idle_period_deadline_ = base::TimeTicks();
}

// static
bool SchedulerHelper::IsInIdlePeriod(IdlePeriodState state) {
  return state != IdlePeriodState::NOT_IN_IDLE_PERIOD;
}

bool SchedulerHelper::CanExceedIdleDeadlineIfRequired() const {
  TRACE_EVENT_BEGIN0(tracing_category_, "CanExceedIdleDeadlineIfRequired");
  CheckOnValidThread();
  return idle_period_state_ ==
         IdlePeriodState::IN_LONG_IDLE_PERIOD_WITH_MAX_DEADLINE;
}

void SchedulerHelper::SetTimeSourceForTesting(
    scoped_refptr<cc::TestNowSource> time_source) {
  CheckOnValidThread();
  time_source_ = time_source;
  task_queue_manager_->SetTimeSourceForTesting(time_source);
}

void SchedulerHelper::SetWorkBatchSizeForTesting(size_t work_batch_size) {
  CheckOnValidThread();
  task_queue_manager_->SetWorkBatchSize(work_batch_size);
}

base::TimeTicks SchedulerHelper::Now() const {
  return UNLIKELY(time_source_) ? time_source_->Now() : base::TimeTicks::Now();
}

SchedulerHelper::IdlePeriodState
SchedulerHelper::SchedulerIdlePeriodState() const {
  return idle_period_state_;
}

PrioritizingTaskQueueSelector*
SchedulerHelper::SchedulerTaskQueueSelector() const {
  return task_queue_selector_.get();
}

TaskQueueManager* SchedulerHelper::SchedulerTaskQueueManager() const {
  return task_queue_manager_.get();
}

// static
const char* SchedulerHelper::TaskQueueIdToString(QueueId queue_id) {
  switch (queue_id) {
    case DEFAULT_TASK_QUEUE:
      return "default_tq";
    case IDLE_TASK_QUEUE:
      return "idle_tq";
    case CONTROL_TASK_QUEUE:
      return "control_tq";
    case CONTROL_TASK_AFTER_WAKEUP_QUEUE:
      return "control_after_wakeup_tq";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
const char* SchedulerHelper::IdlePeriodStateToString(
    IdlePeriodState idle_period_state) {
  switch (idle_period_state) {
    case IdlePeriodState::NOT_IN_IDLE_PERIOD:
      return "not_in_idle_period";
    case IdlePeriodState::IN_SHORT_IDLE_PERIOD:
      return "in_short_idle_period";
    case IdlePeriodState::IN_LONG_IDLE_PERIOD:
      return "in_long_idle_period";
    case IdlePeriodState::IN_LONG_IDLE_PERIOD_WITH_MAX_DEADLINE:
      return "in_long_idle_period_with_max_deadline";
    case IdlePeriodState::ENDING_LONG_IDLE_PERIOD:
      return "ending_long_idle_period";
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

}  // namespace content
