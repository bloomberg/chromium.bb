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
  throttled_queues_.insert(task_queue);

  task_queue->SetTimeDomain(time_domain_.get());
  task_queue->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  MaybeSchedulePumpThrottledTasksLocked();
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
  MaybeSchedulePumpThrottledTasksLocked();
}

void ThrottlingHelper::OnTimeDomainHasDelayedWork() {
  TRACE_EVENT0(tracing_category_,
               "ThrottlingHelper::OnTimeDomainHasDelayedWork");
  MaybeSchedulePumpThrottledTasksLocked();
}

void ThrottlingHelper::PumpThrottledTasks() {
  TRACE_EVENT0(tracing_category_, "ThrottlingHelper::PumpThrottledTasks");
  pending_pump_throttled_tasks_ = false;

  time_domain_->AdvanceTo(tick_clock_->NowTicks());
  bool work_to_do = false;
  for (TaskQueue* task_queue : throttled_queues_) {
    if (task_queue->IsEmpty())
      continue;

    work_to_do = true;
    task_queue->PumpQueue();
  }

  if (work_to_do)
    MaybeSchedulePumpThrottledTasksLocked();
}

/* static */
base::TimeDelta ThrottlingHelper::DelayToNextRunTimeInSeconds(
    base::TimeTicks now) {
  const base::TimeDelta one_second = base::TimeDelta::FromSeconds(1);
  return one_second - ((now - base::TimeTicks()) % one_second);
}

void ThrottlingHelper::MaybeSchedulePumpThrottledTasksLocked() {
  if (pending_pump_throttled_tasks_)
    return;

  pending_pump_throttled_tasks_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE, pump_throttled_tasks_closure_,
      DelayToNextRunTimeInSeconds(tick_clock_->NowTicks()));
}

}  // namespace scheduler
