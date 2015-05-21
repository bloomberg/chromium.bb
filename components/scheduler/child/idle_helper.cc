// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/idle_helper.h"

#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/child/scheduler_helper.h"

namespace scheduler {

IdleHelper::IdleHelper(
    SchedulerHelper* helper,
    Delegate* delegate,
    size_t idle_queue_index,
    const char* tracing_category,
    const char* disabled_by_default_tracing_category,
    const char* idle_period_tracing_name,
    base::TimeDelta required_quiescence_duration_before_long_idle_period)
    : helper_(helper),
      delegate_(delegate),
      idle_queue_index_(idle_queue_index),
      idle_period_state_(IdlePeriodState::NOT_IN_IDLE_PERIOD),
      quiescence_monitored_task_queue_mask_(
          helper_->GetQuiescenceMonitoredTaskQueueMask() &
          ~(1ull << idle_queue_index_)),
      required_quiescence_duration_before_long_idle_period_(
          required_quiescence_duration_before_long_idle_period),
      tracing_category_(tracing_category),
      disabled_by_default_tracing_category_(
          disabled_by_default_tracing_category),
      idle_period_tracing_name_(idle_period_tracing_name),
      weak_factory_(this) {
  weak_idle_helper_ptr_ = weak_factory_.GetWeakPtr();
  end_idle_period_closure_.Reset(
      base::Bind(&IdleHelper::EndIdlePeriod, weak_idle_helper_ptr_));
  enable_next_long_idle_period_closure_.Reset(
      base::Bind(&IdleHelper::EnableLongIdlePeriod, weak_idle_helper_ptr_));
  enable_next_long_idle_period_after_wakeup_closure_.Reset(base::Bind(
      &IdleHelper::EnableLongIdlePeriodAfterWakeup, weak_idle_helper_ptr_));

  idle_task_runner_ = make_scoped_refptr(new SingleThreadIdleTaskRunner(
      helper_->TaskRunnerForQueue(idle_queue_index_),
      helper_->ControlAfterWakeUpTaskRunner(),
      base::Bind(&IdleHelper::CurrentIdleTaskDeadlineCallback,
                 weak_idle_helper_ptr_),
      tracing_category));

  helper_->DisableQueue(idle_queue_index_);
  helper_->SetPumpPolicy(idle_queue_index_,
                         TaskQueueManager::PumpPolicy::MANUAL);
}

IdleHelper::~IdleHelper() {
}

IdleHelper::Delegate::Delegate() {
}

IdleHelper::Delegate::~Delegate() {
}

scoped_refptr<SingleThreadIdleTaskRunner> IdleHelper::IdleTaskRunner() {
  helper_->CheckOnValidThread();
  return idle_task_runner_;
}

void IdleHelper::CurrentIdleTaskDeadlineCallback(
    base::TimeTicks* deadline_out) const {
  helper_->CheckOnValidThread();
  *deadline_out = idle_period_deadline_;
}

IdleHelper::IdlePeriodState IdleHelper::ComputeNewLongIdlePeriodState(
    const base::TimeTicks now,
    base::TimeDelta* next_long_idle_period_delay_out) {
  helper_->CheckOnValidThread();

  if (!delegate_->CanEnterLongIdlePeriod(now,
                                         next_long_idle_period_delay_out)) {
    return IdlePeriodState::NOT_IN_IDLE_PERIOD;
  }

  base::TimeTicks next_pending_delayed_task =
      helper_->NextPendingDelayedTaskRunTime();
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
        kRetryEnableLongIdlePeriodDelayMillis);
    return IdlePeriodState::NOT_IN_IDLE_PERIOD;
  }
}

bool IdleHelper::ShouldWaitForQuiescence() {
  helper_->CheckOnValidThread();

  if (helper_->IsShutdown())
    return false;

  if (required_quiescence_duration_before_long_idle_period_ ==
      base::TimeDelta())
    return false;

  uint64 task_queues_run_since_last_check_bitmap =
      helper_->GetAndClearTaskWasRunOnQueueBitmap() &
      quiescence_monitored_task_queue_mask_;

  TRACE_EVENT1(disabled_by_default_tracing_category_, "ShouldWaitForQuiescence",
               "task_queues_run_since_last_check_bitmap",
               task_queues_run_since_last_check_bitmap);

  // If anything was run on the queues we care about, then we're not quiescent
  // and we should wait.
  return task_queues_run_since_last_check_bitmap != 0;
}

void IdleHelper::EnableLongIdlePeriod() {
  TRACE_EVENT0(disabled_by_default_tracing_category_, "EnableLongIdlePeriod");
  helper_->CheckOnValidThread();

  // End any previous idle period.
  EndIdlePeriod();

  if (ShouldWaitForQuiescence()) {
    helper_->ControlTaskRunner()->PostDelayedTask(
        FROM_HERE, enable_next_long_idle_period_closure_.callback(),
        required_quiescence_duration_before_long_idle_period_);
    delegate_->IsNotQuiescent();
    return;
  }

  base::TimeTicks now(helper_->Now());
  base::TimeDelta next_long_idle_period_delay;
  IdlePeriodState new_idle_period_state =
      ComputeNewLongIdlePeriodState(now, &next_long_idle_period_delay);
  if (IsInIdlePeriod(new_idle_period_state)) {
    StartIdlePeriod(new_idle_period_state, now,
                    now + next_long_idle_period_delay, false);
  }

  if (helper_->IsQueueEmpty(idle_queue_index_)) {
    // If there are no current idle tasks then post the call to initiate the
    // next idle for execution after wakeup (at which point after-wakeup idle
    // tasks might be eligible to run or more idle tasks posted).
    helper_->ControlAfterWakeUpTaskRunner()->PostDelayedTask(
        FROM_HERE,
        enable_next_long_idle_period_after_wakeup_closure_.callback(),
        next_long_idle_period_delay);
  } else {
    // Otherwise post on the normal control task queue.
    helper_->ControlTaskRunner()->PostDelayedTask(
        FROM_HERE, enable_next_long_idle_period_closure_.callback(),
        next_long_idle_period_delay);
  }
}

void IdleHelper::EnableLongIdlePeriodAfterWakeup() {
  TRACE_EVENT0(disabled_by_default_tracing_category_,
               "EnableLongIdlePeriodAfterWakeup");
  helper_->CheckOnValidThread();

  if (IsInIdlePeriod(idle_period_state_)) {
    // Since we were asleep until now, end the async idle period trace event at
    // the time when it would have ended were we awake.
    TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP0(
        tracing_category_, idle_period_tracing_name_, this,
        std::min(idle_period_deadline_, helper_->Now()).ToInternalValue());
    idle_period_state_ = IdlePeriodState::ENDING_LONG_IDLE_PERIOD;
    EndIdlePeriod();
  }

  // Post a task to initiate the next long idle period rather than calling it
  // directly to allow all pending PostIdleTaskAfterWakeup tasks to get enqueued
  // on the idle task queue before the next idle period starts so they are
  // eligible to be run during the new idle period.
  helper_->ControlTaskRunner()->PostTask(
      FROM_HERE, enable_next_long_idle_period_closure_.callback());
}

void IdleHelper::StartIdlePeriod(IdlePeriodState new_state,
                                 base::TimeTicks now,
                                 base::TimeTicks idle_period_deadline,
                                 bool post_end_idle_period) {
  DCHECK_GT(idle_period_deadline, now);
  TRACE_EVENT_ASYNC_BEGIN0(tracing_category_, idle_period_tracing_name_, this);
  helper_->CheckOnValidThread();
  DCHECK(IsInIdlePeriod(new_state));

  helper_->EnableQueue(idle_queue_index_,
                       PrioritizingTaskQueueSelector::BEST_EFFORT_PRIORITY);
  helper_->PumpQueue(idle_queue_index_);
  idle_period_state_ = new_state;

  idle_period_deadline_ = idle_period_deadline;
  if (post_end_idle_period) {
    helper_->ControlTaskRunner()->PostDelayedTask(
        FROM_HERE, end_idle_period_closure_.callback(),
        idle_period_deadline_ - now);
  }
}

void IdleHelper::EndIdlePeriod() {
  helper_->CheckOnValidThread();

  end_idle_period_closure_.Cancel();
  enable_next_long_idle_period_closure_.Cancel();
  enable_next_long_idle_period_after_wakeup_closure_.Cancel();

  // If we weren't already within an idle period then early-out.
  if (!IsInIdlePeriod(idle_period_state_))
    return;

  // If we are in the ENDING_LONG_IDLE_PERIOD state we have already logged the
  // trace event.
  if (idle_period_state_ != IdlePeriodState::ENDING_LONG_IDLE_PERIOD) {
    bool is_tracing;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(tracing_category_, &is_tracing);
    if (is_tracing && !idle_period_deadline_.is_null() &&
        helper_->Now() > idle_period_deadline_) {
      TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0(
          tracing_category_, idle_period_tracing_name_, this, "DeadlineOverrun",
          idle_period_deadline_.ToInternalValue());
    }
    TRACE_EVENT_ASYNC_END0(tracing_category_, idle_period_tracing_name_, this);
  }

  helper_->DisableQueue(idle_queue_index_);
  idle_period_state_ = IdlePeriodState::NOT_IN_IDLE_PERIOD;
  idle_period_deadline_ = base::TimeTicks();
}

// static
bool IdleHelper::IsInIdlePeriod(IdlePeriodState state) {
  return state != IdlePeriodState::NOT_IN_IDLE_PERIOD;
}

bool IdleHelper::CanExceedIdleDeadlineIfRequired() const {
  TRACE_EVENT0(tracing_category_, "CanExceedIdleDeadlineIfRequired");
  helper_->CheckOnValidThread();
  return idle_period_state_ ==
         IdlePeriodState::IN_LONG_IDLE_PERIOD_WITH_MAX_DEADLINE;
}

IdleHelper::IdlePeriodState IdleHelper::SchedulerIdlePeriodState() const {
  return idle_period_state_;
}

// static
const char* IdleHelper::IdlePeriodStateToString(
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

}  // namespace scheduler
