// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/throttled_time_domain.h"

#include "base/time/tick_clock.h"

namespace scheduler {

ThrottledTimeDomain::ThrottledTimeDomain(TimeDomain::Observer* observer,
                                         base::TickClock* tick_clock)
    : VirtualTimeDomain(observer, tick_clock->NowTicks()),
      tick_clock_(tick_clock) {}

ThrottledTimeDomain::~ThrottledTimeDomain() {}

base::TimeTicks ThrottledTimeDomain::ComputeDelayedRunTime(
    base::TimeTicks,
    base::TimeDelta delay) const {
  // We ignore the |time_domain_now| parameter since its the virtual time but we
  // need to use the current real time when computing the delayed runtime.  If
  // don't do that, throttled timers may fire sooner than expected.
  return tick_clock_->NowTicks() + delay;
}

const char* ThrottledTimeDomain::GetName() const {
  return "ThrottledTimeDomain";
}

}  // namespace scheduler
