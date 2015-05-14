// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TASK_SWITCH_TIME_TRACKER_TEST_API_H_
#define ASH_TEST_TASK_SWITCH_TIME_TRACKER_TEST_API_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"

namespace base {
class SimpleTestTickClock;
}  // namespace base

namespace ash {

class TaskSwitchTimeTracker;

namespace test {

// Provides access TaskSwitchTimeTracker's internals.
class TaskSwitchTimeTrackerTestAPI {
 public:
  // Creates a TaskSwitchTimeTracker with the given |histogram_name| and injects
  // a base::SimpleTestTickClock that can be controlled.
  explicit TaskSwitchTimeTrackerTestAPI(const std::string& histogram_name);

  ~TaskSwitchTimeTrackerTestAPI();

  TaskSwitchTimeTracker* time_tracker() { return time_tracker_.get(); }

  // Advances |time_tracker_|'s TickClock by |time_delta|.
  void Advance(base::TimeDelta time_delta);

  // Wrapper function to access TaskSwitchTimeTracker::HasLastActionTime();
  bool HasLastActionTime() const;

 private:
  // Owned by |time_tracker_|.
  base::SimpleTestTickClock* tick_clock_;

  // The TaskSwitchTimeTracker to provide internal access to.
  scoped_ptr<TaskSwitchTimeTracker> time_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TaskSwitchTimeTrackerTestAPI);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TASK_SWITCH_TIME_TRACKER_TEST_API_H_
