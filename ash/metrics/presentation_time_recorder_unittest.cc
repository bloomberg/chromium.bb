// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/presentation_time_recorder.h"

#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/views/widget/widget.h"

namespace ash {

using PresentationTimeRecorderTest = ash::AshTestBase;

constexpr char kName[] = "Histogram";
constexpr char kMaxLatencyName[] = "MaxLatency.Histogram";

TEST_F(PresentationTimeRecorderTest, Histogram) {
  base::HistogramTester histogram_tester;

  auto* compositor = CurrentContext()->layer()->GetCompositor();
  auto test_recorder = std::make_unique<PresentationTimeHistogramRecorder>(
      compositor, kName, kMaxLatencyName);

  // Flush pending draw requests.
  for (int i = 0; i < 30; i++) {
    compositor->ScheduleFullRedraw();
    WaitForNextFrameToBePresented(compositor);
    base::RunLoop().RunUntilIdle();
  }

  compositor->ScheduleFullRedraw();
  histogram_tester.ExpectTotalCount(kName, 0);
  test_recorder->RequestNext();
  histogram_tester.ExpectTotalCount(kName, 0);
  test_recorder->RequestNext();
  histogram_tester.ExpectTotalCount(kName, 0);
  histogram_tester.ExpectTotalCount(kMaxLatencyName, 0);

  WaitForNextFrameToBePresented(compositor);
  histogram_tester.ExpectTotalCount(kName, 1);
  histogram_tester.ExpectTotalCount(kMaxLatencyName, 0);

  compositor->ScheduleFullRedraw();
  WaitForNextFrameToBePresented(compositor);
  histogram_tester.ExpectTotalCount(kName, 1);
  histogram_tester.ExpectTotalCount(kMaxLatencyName, 0);

  test_recorder->RequestNext();
  compositor->ScheduleFullRedraw();
  WaitForNextFrameToBePresented(compositor);
  histogram_tester.ExpectTotalCount(kName, 2);
  histogram_tester.ExpectTotalCount(kMaxLatencyName, 0);

  // Drawing without RequestNext should not affect histogram.
  compositor->ScheduleFullRedraw();
  WaitForNextFrameToBePresented(compositor);
  histogram_tester.ExpectTotalCount(kName, 2);
  histogram_tester.ExpectTotalCount(kMaxLatencyName, 0);

  // Make sure the max latency is recorded upon deletion.
  test_recorder.reset();
  histogram_tester.ExpectTotalCount(kName, 2);
  histogram_tester.ExpectTotalCount(kMaxLatencyName, 1);
}

TEST_F(PresentationTimeRecorderTest, Failure) {
  auto* compositor = CurrentContext()->layer()->GetCompositor();
  auto test_recorder = std::make_unique<PresentationTimeHistogramRecorder>(
      compositor, kName, kMaxLatencyName);

  test_recorder->RequestNext();
  test_recorder->OnCompositingDidCommit(compositor);
  base::TimeDelta interval_not_used = base::TimeDelta::FromMilliseconds(0);
  base::TimeTicks start = base::TimeTicks::FromUptimeMillis(1000);
  gfx::PresentationFeedback success(base::TimeTicks::FromUptimeMillis(1100),
                                    interval_not_used, /*flags=*/0);
  test_recorder->OnPresented(0, start, success);
  EXPECT_EQ(100, test_recorder->max_latency_ms());
  EXPECT_EQ(1, test_recorder->success_count());
  gfx::PresentationFeedback failure(base::TimeTicks::FromUptimeMillis(2000),
                                    interval_not_used,
                                    gfx::PresentationFeedback::kFailure);
  test_recorder->OnPresented(0, start, failure);
  // Failure should not be included in max latency.
  EXPECT_EQ(100, test_recorder->max_latency_ms());
  EXPECT_EQ(50, test_recorder->failure_ratio());
}

}  // namespace ash
