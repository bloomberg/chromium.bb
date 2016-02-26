// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_THROTTLED_TIME_DOMAIN_H_
#define COMPONENTS_SCHEDULER_RENDERER_THROTTLED_TIME_DOMAIN_H_

#include "base/macros.h"
#include "components/scheduler/base/virtual_time_domain.h"

namespace scheduler {

// A time domain that's mostly a VirtualTimeDomain except that real time is used
// when computing the delayed runtime.
class SCHEDULER_EXPORT ThrottledTimeDomain : public VirtualTimeDomain {
 public:
  ThrottledTimeDomain(TimeDomain::Observer* observer,
                      base::TickClock* tick_clock);
  ~ThrottledTimeDomain() override;

  // TimeDomain implementation:
  base::TimeTicks ComputeDelayedRunTime(base::TimeTicks time_domain_now,
                                        base::TimeDelta delay) const override;
  const char* GetName() const override;

 private:
  base::TickClock* const tick_clock_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(ThrottledTimeDomain);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_THROTTLED_TIME_DOMAIN_H_
