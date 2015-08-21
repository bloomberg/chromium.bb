// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/task_cost_estimator.h"

namespace scheduler {

TaskCostEstimator::TaskCostEstimator(int sample_count,
                                     double estimation_percentile)
    : rolling_time_delta_history_(sample_count),
      outstanding_task_count_(0),
      estimation_percentile_(estimation_percentile) {}

TaskCostEstimator::~TaskCostEstimator() {}

void TaskCostEstimator::WillProcessTask(const base::PendingTask& pending_task) {
  // Avoid measuring the duration in nested run loops.
  if (++outstanding_task_count_ == 1)
    task_start_time_ = Now();
}

void TaskCostEstimator::DidProcessTask(const base::PendingTask& pending_task) {
  if (--outstanding_task_count_ == 0) {
    base::TimeDelta duration = Now() - task_start_time_;
    rolling_time_delta_history_.InsertSample(duration);

    // TODO(skyostil): Should we do this less often?
    expected_task_duration_ =
        rolling_time_delta_history_.Percentile(estimation_percentile_);
  }
}

base::TimeTicks TaskCostEstimator::Now() {
  return base::TimeTicks::Now();
}

}  // namespace scheduler
