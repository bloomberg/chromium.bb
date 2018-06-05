// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/task_switch_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Test fixture for the TaskSwitchMetricsRecorder class.
class TaskSwitchMetricsRecorderTest : public testing::Test {
 public:
  TaskSwitchMetricsRecorderTest();
  ~TaskSwitchMetricsRecorderTest() override;

  // Wrapper to the test targets OnTaskSwitch(TaskSwitchSource) method.
  void OnTaskSwitch(TaskSwitchSource task_switch_source);

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 protected:
  // Used to verify recorded data.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  // The test target.
  std::unique_ptr<TaskSwitchMetricsRecorder> task_switch_metrics_recorder_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskSwitchMetricsRecorderTest);
};

TaskSwitchMetricsRecorderTest::TaskSwitchMetricsRecorderTest() = default;

TaskSwitchMetricsRecorderTest::~TaskSwitchMetricsRecorderTest() = default;

void TaskSwitchMetricsRecorderTest::OnTaskSwitch(
    TaskSwitchSource task_switch_source) {
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

// Verifies that task switches from a non ANY source also add data to the
// Ash.TimeBetweenTaskSwitches histogram.
TEST_F(TaskSwitchMetricsRecorderTest,
       VerifyTaskSwitchesRecordInAllTaskSwitchHistogram) {
  const std::string kHistogramName = "Ash.TimeBetweenTaskSwitches";

  OnTaskSwitch(TaskSwitchSource::SHELF);
  OnTaskSwitch(TaskSwitchSource::SHELF);
  histogram_tester_->ExpectTotalCount(kHistogramName, 1);
}

// Verifies that the TaskSwitchSource::DESKTOP source adds data to the
// Ash.Desktop.TimeBetweenNavigateToTaskSwitches histogram.
TEST_F(TaskSwitchMetricsRecorderTest, VerifyTaskSwitchesForDesktopAreRecorded) {
  const std::string kHistogramName =
      "Ash.Desktop.TimeBetweenNavigateToTaskSwitches";

  OnTaskSwitch(TaskSwitchSource::DESKTOP);
  OnTaskSwitch(TaskSwitchSource::DESKTOP);
  histogram_tester_->ExpectTotalCount(kHistogramName, 1);

  OnTaskSwitch(TaskSwitchSource::DESKTOP);
  histogram_tester_->ExpectTotalCount(kHistogramName, 2);
}

// Verifies that the TaskSwitchSource::WINDOW_CYCLE_CONTROLLER source
// adds data to the Ash.WindowCycleController.TimeBetweenTaskSwitches histogram.
TEST_F(TaskSwitchMetricsRecorderTest,
       VerifyTaskSwitchesForWindowCycleControllerAreRecorded) {
  const std::string kHistogramName =
      "Ash.WindowCycleController.TimeBetweenTaskSwitches";

  OnTaskSwitch(TaskSwitchSource::WINDOW_CYCLE_CONTROLLER);
  OnTaskSwitch(TaskSwitchSource::WINDOW_CYCLE_CONTROLLER);
  histogram_tester_->ExpectTotalCount(kHistogramName, 1);

  OnTaskSwitch(TaskSwitchSource::WINDOW_CYCLE_CONTROLLER);
  histogram_tester_->ExpectTotalCount(kHistogramName, 2);
}

// Verifies that the TaskSwitchSource::SHELF source adds data to the
// Ash.Shelf.TimeBetweenNavigateToTaskSwitches histogram.
TEST_F(TaskSwitchMetricsRecorderTest,
       VerifyTaskSwitchesFromTheShelfAreRecorded) {
  const std::string kHistogramName =
      "Ash.Shelf.TimeBetweenNavigateToTaskSwitches";

  OnTaskSwitch(TaskSwitchSource::SHELF);
  OnTaskSwitch(TaskSwitchSource::SHELF);
  histogram_tester_->ExpectTotalCount(kHistogramName, 1);

  OnTaskSwitch(TaskSwitchSource::SHELF);
  histogram_tester_->ExpectTotalCount(kHistogramName, 2);
}

// Verifies that the TaskSwitchSource::OVERVIEW_MODE source adds data
// to the Ash.WindowSelector.TimeBetweenActiveWindowChanges histogram.
TEST_F(TaskSwitchMetricsRecorderTest,
       VerifyTaskSwitchesFromOverviewModeAreRecorded) {
  const std::string kHistogramName =
      "Ash.WindowSelector.TimeBetweenActiveWindowChanges";

  OnTaskSwitch(TaskSwitchSource::OVERVIEW_MODE);
  OnTaskSwitch(TaskSwitchSource::OVERVIEW_MODE);
  histogram_tester_->ExpectTotalCount(kHistogramName, 1);

  OnTaskSwitch(TaskSwitchSource::OVERVIEW_MODE);
  histogram_tester_->ExpectTotalCount(kHistogramName, 2);
}

}  // namespace ash
