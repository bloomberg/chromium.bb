// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/virtual_time_domain.h"

#include "base/bind.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/task_queue_manager_delegate.h"

namespace scheduler {

VirtualTimeDomain::VirtualTimeDomain(base::TimeTicks initial_time)
    : now_(initial_time) {}

VirtualTimeDomain::~VirtualTimeDomain() {}

LazyNow VirtualTimeDomain::CreateLazyNow() {
  base::AutoLock lock(lock_);
  return LazyNow(now_);
}

void VirtualTimeDomain::RequestWakeup(base::TimeDelta delay) {
  // We don't need to do anything here because AdvanceTo triggers delayed tasks.
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
  LazyNow lazy_now(now_);
  WakeupReadyDelayedQueues(&lazy_now);
}

const char* VirtualTimeDomain::GetName() const {
  return "VirtualTimeDomain";
}

}  // namespace scheduler
