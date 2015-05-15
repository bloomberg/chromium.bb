// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/task_switch_metrics_recorder.h"

#include "base/test/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Test fixture for the TaskSwitchMetricsRecorder class.
class TaskSwitchMetricsRecorderTest : public testing::Test {
 public:
  TaskSwitchMetricsRecorderTest();
  ~TaskSwitchMetricsRecorderTest() override;

  // Wrapper to the test targets OnTaskSwitch(TaskSwitchSource) method.
  void OnTaskSwitch(
      TaskSwitchMetricsRecorder::TaskSwitchSource task_switch_source);

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 protected:
  // Used to verify recorded data.
  scoped_ptr<base::HistogramTester> histogram_tester_;

  // The test target.
  scoped_ptr<TaskSwitchMetricsRecorder> task_switch_metrics_recorder_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskSwitchMetricsRecorderTest);
};

TaskSwitchMetricsRecorderTest::TaskSwitchMetricsRecorderTest() {
}

TaskSwitchMetricsRecorderTest::~TaskSwitchMetricsRecorderTest() {
}

void TaskSwitchMetricsRecorderTest::OnTaskSwitch(
    TaskSwitchMetricsRecorder::TaskSwitchSource task_switch_source) {
  task_switch_metrics_recorder_->OnTaskSwitch(task_switch_source);
}

void TaskSwitchMetricsRecorderTest::SetUp() {
  testing::Test::SetUp();

  histogram_tester_.reset(new base::HistogramTester());
  task_switch_metrics_recorder_.reset(new TaskSwitchMetricsRecorder());
}

void TaskSwitchMetricsRecorderTest::TearDown() {
  testing::Test::TearDown();

  histogram_tester_.reset();
  task_switch_metrics_recorder_.reset();
}

}  // namespace

// Verifies that the TaskSwitchMetricsRecorder::kWindowCycleController source
// adds data to the Ash.WindowCycleController.TimeBetweenTaskSwitches histogram.
TEST_F(TaskSwitchMetricsRecorderTest,
       VerifyTaskSwitchesForWindowCycleControllerAreRecorded) {
  const std::string kHistogramName =
      "Ash.WindowCycleController.TimeBetweenTaskSwitches";

  OnTaskSwitch(TaskSwitchMetricsRecorder::kWindowCycleController);
  OnTaskSwitch(TaskSwitchMetricsRecorder::kWindowCycleController);
  histogram_tester_->ExpectTotalCount(kHistogramName, 1);

  OnTaskSwitch(TaskSwitchMetricsRecorder::kWindowCycleController);
  histogram_tester_->ExpectTotalCount(kHistogramName, 2);
}

// Verifies that the TaskSwitchMetricsRecorder::kShelf source adds data to the
// Ash.Shelf.TimeBetweenNavigateToTaskSwitches histogram.
TEST_F(TaskSwitchMetricsRecorderTest,
       VerifyTaskSwitchesFromTheShelfAreRecorded) {
  const std::string kHistogramName =
      "Ash.Shelf.TimeBetweenNavigateToTaskSwitches";

  OnTaskSwitch(TaskSwitchMetricsRecorder::kShelf);
  OnTaskSwitch(TaskSwitchMetricsRecorder::kShelf);
  histogram_tester_->ExpectTotalCount(kHistogramName, 1);

  OnTaskSwitch(TaskSwitchMetricsRecorder::kShelf);
  histogram_tester_->ExpectTotalCount(kHistogramName, 2);
}

// Verifies that the TaskSwitchMetricsRecorder::kTabStrip source adds data to
// the Ash.Tab.TimeBetweenSwitchToExistingTabUserActions histogram.
TEST_F(TaskSwitchMetricsRecorderTest,
       VerifyTaskSwitchesFromTheTabStripAreRecorded) {
  const std::string kHistogramName =
      "Ash.Tab.TimeBetweenSwitchToExistingTabUserActions";

  OnTaskSwitch(TaskSwitchMetricsRecorder::kTabStrip);
  OnTaskSwitch(TaskSwitchMetricsRecorder::kTabStrip);
  histogram_tester_->ExpectTotalCount(kHistogramName, 1);

  OnTaskSwitch(TaskSwitchMetricsRecorder::kTabStrip);
  histogram_tester_->ExpectTotalCount(kHistogramName, 2);
}

}  // namespace ash
