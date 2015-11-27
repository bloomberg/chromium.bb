// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/virtual_time_domain.h"

#include "base/bind.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/task_queue_manager_delegate.h"

namespace scheduler {

VirtualTimeDomain::VirtualTimeDomain(TimeDomain::Observer* observer,
                                     base::TimeTicks initial_time)
    : TimeDomain(observer), now_(initial_time) {}

VirtualTimeDomain::~VirtualTimeDomain() {}

void VirtualTimeDomain::OnRegisterWithTaskQueueManager(
    TaskQueueManagerDelegate* task_queue_manager_delegate,
    base::Closure do_work_closure) {
  task_queue_manager_delegate_ = task_queue_manager_delegate;
  do_work_closure_ = do_work_closure;
  DCHECK(task_queue_manager_delegate_);
}

LazyNow VirtualTimeDomain::CreateLazyNow() {
  base::AutoLock lock(lock_);
  return LazyNow(now_);
}

void VirtualTimeDomain::RequestWakeup(LazyNow* lazy_now,
                                      base::TimeDelta delay) {
  // We don't need to do anything here because AdvanceTo posts a DoWork.
}

bool VirtualTimeDomain::MaybeAdvanceTime() {
  return false;
}

void VirtualTimeDomain::AsValueIntoInternal(
    base::trace_event::TracedValue* state) const {}

void VirtualTimeDomain::AdvanceTo(base::TimeTicks now) {
  base::AutoLock lock(lock_);
  DCHECK_GE(now, now_);
  now_ = now;
  DCHECK(task_queue_manager_delegate_);

  task_queue_manager_delegate_->PostTask(FROM_HERE, do_work_closure_);
}

const char* VirtualTimeDomain::GetName() const {
  return "VirtualTimeDomain";
}

}  // namespace scheduler
