// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/real_time_domain.h"

#include "base/bind.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/task_queue_manager_delegate.h"

namespace scheduler {

RealTimeDomain::RealTimeDomain(
    TaskQueueManagerDelegate* task_queue_manager_delegate,
    base::Closure do_work_closure)
    : task_queue_manager_delegate_(task_queue_manager_delegate),
      do_work_closure_(do_work_closure) {
  DCHECK(task_queue_manager_delegate_);
}

RealTimeDomain::~RealTimeDomain() {}

LazyNow RealTimeDomain::CreateLazyNow() {
  return LazyNow(task_queue_manager_delegate_);
}

void RealTimeDomain::RequestWakeup(base::TimeDelta delay) {
  task_queue_manager_delegate_->PostDelayedTask(FROM_HERE, do_work_closure_,
                                                delay);
}

bool RealTimeDomain::MaybeAdvanceTime() {
  return false;
}

void RealTimeDomain::AsValueIntoInternal(
    base::trace_event::TracedValue* state) const {}

const char* RealTimeDomain::GetName() const {
  return "RealTimeDomain";
}

}  // namespace scheduler
