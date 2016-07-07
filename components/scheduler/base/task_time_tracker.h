// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_BASE_TASK_TIME_TRACKER_H_
#define CONTENT_RENDERER_SCHEDULER_BASE_TASK_TIME_TRACKER_H_

#include "base/time/time.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

class SCHEDULER_EXPORT TaskTimeTracker {
 public:
  TaskTimeTracker() {}
  virtual ~TaskTimeTracker() {}

  virtual void ReportTaskTime(base::TimeTicks startTime,
                              base::TimeTicks endTime) = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(TaskTimeTracker);
};

} // namespace scheduler

#endif // CONTENT_RENDERER_SCHEDULER_BASE_TASK_TIME_TRACKER_H_
