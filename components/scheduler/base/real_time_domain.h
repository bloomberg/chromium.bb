// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_BASE_REAL_TIME_DOMAIN_H_
#define COMPONENTS_SCHEDULER_BASE_REAL_TIME_DOMAIN_H_

#include <set>

#include "base/macros.h"
#include "components/scheduler/base/time_domain.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {
class TaskQueueManagerDelegate;

class SCHEDULER_EXPORT RealTimeDomain : public TimeDomain {
 public:
  RealTimeDomain();
  ~RealTimeDomain() override;

  // TimeDomain implementation:
  LazyNow CreateLazyNow() override;
  bool MaybeAdvanceTime() override;
  const char* GetName() const override;

 protected:
  void OnRegisterWithTaskQueueManager(
      TaskQueueManagerDelegate* task_queue_manager_delegate,
      base::Closure do_work_closure) override;
  void RequestWakeup(LazyNow* lazy_now, base::TimeDelta delay) override;
  void AsValueIntoInternal(
      base::trace_event::TracedValue* state) const override;

 private:
  void PostWrappedDoWork(base::TimeTicks now, base::TimeTicks run_time);
  void WrappedDoWorkTask(base::TimeTicks run_time);

  TaskQueueManagerDelegate* task_queue_manager_delegate_;  // NOT OWNED
  std::set<base::TimeTicks> pending_wakeups_;
  base::Closure do_work_closure_;
  base::WeakPtrFactory<RealTimeDomain> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RealTimeDomain);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_BASE_REAL_TIME_DOMAIN_H_
