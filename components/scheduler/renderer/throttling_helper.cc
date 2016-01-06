// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/throttling_helper.h"

#include "base/logging.h"
#include "components/scheduler/base/real_time_domain.h"
#include "components/scheduler/base/virtual_time_domain.h"
#include "components/scheduler/child/scheduler_tqm_delegate.h"
#include "components/scheduler/renderer/renderer_scheduler_impl.h"
#include "components/scheduler/renderer/web_frame_scheduler_impl.h"
#include "third_party/WebKit/public/platform/WebFrameScheduler.h"

namespace scheduler {

ThrottlingHelper::ThrottlingHelper(RendererSchedulerImpl* renderer_scheduler,
                                   const char* tracing_category)
    : task_runner_(renderer_scheduler->ControlTaskRunner()),
      renderer_scheduler_(renderer_scheduler),
      tick_clock_(renderer_scheduler->tick_clock()),
      tracing_category_(tracing_category),
      time_domain_(new VirtualTimeDomain(this, tick_clock_->NowTicks())),
      pending_pump_throttled_tasks_(false),
      weak_factory_(this) {
  pump_throttled_tasks_closure_ = base::Bind(
      &ThrottlingHelper::PumpThrottledTasks, weak_factory_.GetWeakPtr());
  forward_immediate_work_closure_ =
      base::Bind(&ThrottlingHelper::OnTimeDomainHasImmediateWork,
                 weak_factory_.GetWeakPtr());
  renderer_scheduler_->RegisterTimeDomain(time_domain_.get());
}

ThrottlingHelper::~ThrottlingHelper() {
  renderer_scheduler_->UnregisterTimeDomain(time_domain_.get());
}

void ThrottlingHelper::Throttle(TaskQueue* task_queue) {
  DCHECK_NE(task_queue, task_runner_.get());
  throttled_queues_.insert(task_queue);

  task_queue->SetTimeDomain(time_domain_.get());
  task_queue->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  if (!task_queue->IsEmpty())
    MaybeSchedulePumpThrottledTasksLocked(FROM_HERE);
}

void ThrottlingHelper::Unthrottle(TaskQueue* task_queue) {
  throttled_queues_.erase(task_queue);

  task_queue->SetTimeDomain(renderer_scheduler_->real_time_domain());
  task_queue->SetPumpPolicy(TaskQueue::PumpPolicy::AUTO);
}

void ThrottlingHelper::OnTimeDomainHasImmediateWork() {
  // Forward to the main thread if called from another thread.
  if (!task_runner_->RunsTasksOnCurrentThread()) {
    task_runner_->PostTask(FROM_HERE, forward_immediate_work_closure_);
    return;
  }
  TRACE_EVENT0(tracing_category_,
               "ThrottlingHelper::OnTimeDomainHasImmediateWork");
  MaybeSchedulePumpThrottledTasksLocked(FROM_HERE);
}

void ThrottlingHelper::OnTimeDomainHasDelayedWork() {
  TRACE_EVENT0(tracing_category_,
               "ThrottlingHelper::OnTimeDomainHasDelayedWork");
  MaybeSchedulePumpThrottledTasksLocked(FROM_HERE);
}

void ThrottlingHelper::PumpThrottledTasks() {
  TRACE_EVENT0(tracing_category_, "ThrottlingHelper::PumpThrottledTasks");
  pending_pump_throttled_tasks_ = false;

  time_domain_->AdvanceTo(tick_clock_->NowTicks());
  for (TaskQueue* task_queue : throttled_queues_) {
    if (task_queue->IsEmpty())
      continue;

    task_queue->PumpQueue(false);
  }
  // Make sure NextScheduledRunTime gives us an up-to date result.
  time_domain_->ClearExpiredWakeups();

  base::TimeTicks next_scheduled_delayed_task;
  // Maybe schedule a call to ThrottlingHelper::PumpThrottledTasks if there is
  // a pending delayed task. NOTE posting a non-delayed task in the future will
  // result in ThrottlingHelper::OnTimeDomainHasImmediateWork being called.
  //
  // TODO(alexclarke): Consider taking next_scheduled_delayed_task into account
  // inside MaybeSchedulePumpThrottledTasksLocked.
  if (time_domain_->NextScheduledRunTime(&next_scheduled_delayed_task))
    MaybeSchedulePumpThrottledTasksLocked(FROM_HERE);
}

/* static */
base::TimeDelta ThrottlingHelper::DelayToNextRunTimeInSeconds(
    base::TimeTicks now) {
  const base::TimeDelta one_second = base::TimeDelta::FromSeconds(1);
  return one_second - ((now - base::TimeTicks()) % one_second);
}

void ThrottlingHelper::MaybeSchedulePumpThrottledTasksLocked(
    const tracked_objects::Location& from_here) {
  if (pending_pump_throttled_tasks_)
    return;

  pending_pump_throttled_tasks_ = true;
  task_runner_->PostDelayedTask(
      from_here, pump_throttled_tasks_closure_,
      DelayToNextRunTimeInSeconds(tick_clock_->NowTicks()));
}

}  // namespace scheduler
