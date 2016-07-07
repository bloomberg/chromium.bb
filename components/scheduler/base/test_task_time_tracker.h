// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_BASE_TEST_TASK_TIME_TRACKER_H_
#define CONTENT_RENDERER_SCHEDULER_BASE_TEST_TASK_TIME_TRACKER_H_

#include "base/time/time.h"
#include "components/scheduler/base/task_time_tracker.h"

namespace scheduler {

class TestTaskTimeTracker : public TaskTimeTracker {
 public:
  void ReportTaskTime(base::TimeTicks startTime,
                      base::TimeTicks endTime) override {}
};

} // namespace scheduler

#endif // CONTENT_RENDERER_SCHEDULER_BASE_TEST_TASK_TIME_TRACKER_H_
