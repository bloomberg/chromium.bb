// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_tick_clock.h"
#include "components/scheduler/child/test_time_source.h"
#include "components/scheduler/renderer/task_cost_estimator.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scheduler {

class TaskCostEstimatorTest : public testing::Test {
 public:
  TaskCostEstimatorTest() {}
  ~TaskCostEstimatorTest() override {}

  void SetUp() override {}

  base::SimpleTestTickClock clock_;
};

class TaskCostEstimatorForTest : public TaskCostEstimator {
 public:
  TaskCostEstimatorForTest(base::SimpleTestTickClock* clock,
                           int sample_count,
                           double estimation_percentile)
      : TaskCostEstimator(sample_count, estimation_percentile) {
    SetTimeSourceForTesting(make_scoped_ptr(new TestTimeSource(clock)));
  }
};

TEST_F(TaskCostEstimatorTest, BasicEstimation) {
  TaskCostEstimatorForTest estimator(&clock_, 1, 100);
  base::PendingTask task(FROM_HERE, base::Closure());

  estimator.WillProcessTask(task);
  clock_.Advance(base::TimeDelta::FromMilliseconds(500));
  estimator.DidProcessTask(task);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(500),
            estimator.expected_task_duration());
}

TEST_F(TaskCostEstimatorTest, Clear) {
  TaskCostEstimatorForTest estimator(&clock_, 1, 100);
  base::PendingTask task(FROM_HERE, base::Closure());

  estimator.WillProcessTask(task);
  clock_.Advance(base::TimeDelta::FromMilliseconds(500));
  estimator.DidProcessTask(task);

  estimator.Clear();

  EXPECT_EQ(base::TimeDelta(), estimator.expected_task_duration());
}

TEST_F(TaskCostEstimatorTest, NestedRunLoop) {
  TaskCostEstimatorForTest estimator(&clock_, 1, 100);
  base::PendingTask task(FROM_HERE, base::Closure());

  // Make sure we ignore the tasks inside the nested run loop.
  estimator.WillProcessTask(task);
  estimator.WillProcessTask(task);
  clock_.Advance(base::TimeDelta::FromMilliseconds(500));
  estimator.DidProcessTask(task);
  clock_.Advance(base::TimeDelta::FromMilliseconds(500));
  estimator.DidProcessTask(task);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1000),
            estimator.expected_task_duration());
}

}  // namespace scheduler
