// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/task_switch_time_tracker_test_api.h"

#include "ash/metrics/task_switch_time_tracker.h"
#include "base/test/simple_test_tick_clock.h"

namespace ash {
namespace test {

TaskSwitchTimeTrackerTestAPI::TaskSwitchTimeTrackerTestAPI(
    const std::string& histogram_name) {
  tick_clock_ = new base::SimpleTestTickClock();
  time_tracker_.reset(new TaskSwitchTimeTracker(
      histogram_name, scoped_ptr<base::TickClock>(tick_clock_)));
}

TaskSwitchTimeTrackerTestAPI::~TaskSwitchTimeTrackerTestAPI() {
  tick_clock_ = nullptr;
}

void TaskSwitchTimeTrackerTestAPI::Advance(base::TimeDelta time_delta) {
  tick_clock_->Advance(time_delta);
}

bool TaskSwitchTimeTrackerTestAPI::HasLastActionTime() const {
  return time_tracker_->HasLastActionTime();
}

}  // namespace test
}  // namespace ash
