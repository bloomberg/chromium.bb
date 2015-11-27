// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/real_time_domain.h"

#include "base/bind.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/task_queue_manager_delegate.h"

namespace scheduler {

RealTimeDomain::RealTimeDomain() : TimeDomain(nullptr), weak_factory_(this) {}

RealTimeDomain::~RealTimeDomain() {}

void RealTimeDomain::OnRegisterWithTaskQueueManager(
    TaskQueueManagerDelegate* task_queue_manager_delegate,
    base::Closure do_work_closure) {
  task_queue_manager_delegate_ = task_queue_manager_delegate;
  do_work_closure_ = do_work_closure;
  DCHECK(task_queue_manager_delegate_);
}

LazyNow RealTimeDomain::CreateLazyNow() {
  DCHECK(task_queue_manager_delegate_);
  return LazyNow(task_queue_manager_delegate_);
}

void RealTimeDomain::RequestWakeup(LazyNow* lazy_now, base::TimeDelta delay) {
  PostWrappedDoWork(lazy_now->Now(), lazy_now->Now() + delay);
}

bool RealTimeDomain::MaybeAdvanceTime() {
  base::TimeTicks next_run_time;
  if (!NextScheduledRunTime(&next_run_time))
    return false;

  DCHECK(task_queue_manager_delegate_);
  base::TimeTicks now = task_queue_manager_delegate_->NowTicks();
  if (now >= next_run_time)
    return true;

  PostWrappedDoWork(now, next_run_time);
  return false;
}

void RealTimeDomain::PostWrappedDoWork(base::TimeTicks now,
                                       base::TimeTicks run_time) {
  DCHECK_GE(run_time, now);
  DCHECK(task_queue_manager_delegate_);
  if (pending_wakeups_.insert(run_time).second) {
    task_queue_manager_delegate_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&RealTimeDomain::WrappedDoWorkTask,
                   weak_factory_.GetWeakPtr(), run_time),
        run_time - now);
  }
}

void RealTimeDomain::WrappedDoWorkTask(base::TimeTicks run_time) {
  pending_wakeups_.erase(run_time);
  do_work_closure_.Run();
}

void RealTimeDomain::AsValueIntoInternal(
    base::trace_event::TracedValue* state) const {}

const char* RealTimeDomain::GetName() const {
  return "RealTimeDomain";
}

}  // namespace scheduler
