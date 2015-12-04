// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/real_time_domain.h"

#include "base/bind.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/task_queue_manager.h"
#include "components/scheduler/base/task_queue_manager_delegate.h"

namespace scheduler {

RealTimeDomain::RealTimeDomain()
    : TimeDomain(nullptr), task_queue_manager_(nullptr) {}

RealTimeDomain::~RealTimeDomain() {}

void RealTimeDomain::OnRegisterWithTaskQueueManager(
    TaskQueueManager* task_queue_manager) {
  task_queue_manager_ = task_queue_manager;
  DCHECK(task_queue_manager_);
}

LazyNow RealTimeDomain::CreateLazyNow() {
  return task_queue_manager_->CreateLazyNow();
}

void RealTimeDomain::RequestWakeup(LazyNow* lazy_now, base::TimeDelta delay) {
  task_queue_manager_->MaybeScheduleDelayedWork(FROM_HERE, lazy_now, delay);
}

bool RealTimeDomain::MaybeAdvanceTime() {
  base::TimeTicks next_run_time;
  if (!NextScheduledRunTime(&next_run_time))
    return false;

  LazyNow lazy_now = task_queue_manager_->CreateLazyNow();
  if (lazy_now.Now() >= next_run_time)
    return true;

  task_queue_manager_->MaybeScheduleDelayedWork(FROM_HERE, &lazy_now,
                                                next_run_time - lazy_now.Now());
  return false;
}

void RealTimeDomain::AsValueIntoInternal(
    base::trace_event::TracedValue* state) const {}

const char* RealTimeDomain::GetName() const {
  return "RealTimeDomain";
}

}  // namespace scheduler
