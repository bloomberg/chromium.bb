// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_DISPATCHER_H_
#define COMPONENTS_DOMAIN_RELIABILITY_DISPATCHER_H_

#include <set>

#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "components/domain_reliability/util.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace domain_reliability {

// Runs tasks during a specified interval. Calling |RunEligibleTasks| gives any
// task a chance to run early (if the minimum delay has already passed); tasks
// that aren't run early will be run once their maximum delay has passed.
//
// (See scheduler.h for an explanation of how the intervals are chosen.)
class DOMAIN_RELIABILITY_EXPORT DomainReliabilityDispatcher {
 public:
  DomainReliabilityDispatcher(MockableTime* time);
  ~DomainReliabilityDispatcher();

  void ScheduleTask(
      const base::Closure& task,
      base::TimeDelta min_delay,
      base::TimeDelta max_delay);

  void RunEligibleTasks();

 private:
  struct Task {
    Task(const base::Closure& closure_p,
         scoped_ptr<MockableTime::Timer> timer_p,
         base::TimeDelta min_delay_p,
         base::TimeDelta max_delay_p);
    ~Task();

    base::Closure closure;
    scoped_ptr<MockableTime::Timer> timer;
    base::TimeDelta min_delay;
    base::TimeDelta max_delay;
    bool eligible;
  };

  void MakeTaskWaiting(Task* task);
  void MakeTaskEligible(Task* task);
  void RunAndDeleteTask(Task* task);

  MockableTime* time_;
  std::set<Task*> tasks_;
  std::set<Task*> eligible_tasks_;
};

}  // namespace domain_reliability

#endif
