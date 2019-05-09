// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/time_to_first_present_recorder.h"

#include "ash/metrics/time_to_first_present_recorder_test_api.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/in_process_browser_test.h"

using TimeToFirstPresentRecorderTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(TimeToFirstPresentRecorderTest, VerifyTimeCalculated) {
  // It's possible that the metric was already recorded.
  base::HistogramTester tester;
  auto counts = tester.GetTotalCountsForPrefix(
      ash::TimeToFirstPresentRecorder::kMetricName);
  if (counts[ash::TimeToFirstPresentRecorder::kMetricName] == 1)
    return;

  // The metric wasn't recorded. Wait for it to be recorded.
  base::RunLoop run_loop;
  ash::TimeToFirstPresentRecorderTestApi::SetTimeToFirstPresentCallback(
      run_loop.QuitClosure());
  run_loop.Run();
  counts = tester.GetTotalCountsForPrefix(
      ash::TimeToFirstPresentRecorder::kMetricName);
  EXPECT_EQ(1, counts[ash::TimeToFirstPresentRecorder::kMetricName]);
}
