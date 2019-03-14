// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "content/common/tab_switch_time_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/presentation_feedback.h"

namespace content {

class TabSwitchTimeRecorderTest : public testing::Test {
 public:
  ~TabSwitchTimeRecorderTest() override {}

  void SetUp() override {
    EXPECT_EQ(
        histogram_tester
            .GetAllSamples("Browser.Tabs.TotalSwitchDuration.WithSavedFrames")
            .size(),
        0ULL);
    EXPECT_EQ(
        histogram_tester
            .GetAllSamples("Browser.Tabs.TotalSwitchDuration.NoSavedFrames")
            .size(),
        0ULL);
  }

 protected:
  size_t GetHistogramSampleSize(bool has_saved_frames) {
    if (has_saved_frames) {
      return histogram_tester
          .GetAllSamples("Browser.Tabs.TotalSwitchDuration.WithSavedFrames")
          .size();
    } else {
      return histogram_tester
          .GetAllSamples("Browser.Tabs.TotalSwitchDuration.NoSavedFrames")
          .size();
    }
  }

  TabSwitchTimeRecorder tab_switch_time_recorder_;
  base::HistogramTester histogram_tester;
};

// Time is properly recorded to histogram when we have saved frames and if we
// have a proper matching BeginTimeRecording and callback execution.
TEST_F(TabSwitchTimeRecorderTest, TimeIsRecordedWithSavedFrames) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.BeginTimeRecording(
      start, true /* has_saved_frames */);
  const auto end = base::TimeTicks::Now();
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);
  EXPECT_EQ(GetHistogramSampleSize(true /* has_saved_frames */), 1ULL);
  EXPECT_EQ(GetHistogramSampleSize(false /* has_saved_frames */), 0ULL);
}

// Time is properly recorded to histogram when we have no saved frame and if we
// have a proper matching BeginTimeRecording and callback execution.
TEST_F(TabSwitchTimeRecorderTest, TimeIsRecordedNoSavedFrame) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.BeginTimeRecording(
      start, false /* has_saved_frames */);
  const auto end = base::TimeTicks::Now();
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);
  EXPECT_EQ(GetHistogramSampleSize(true /* has_saved_frames */), 0ULL);
  EXPECT_EQ(GetHistogramSampleSize(false /* has_saved_frames */), 1ULL);
}

// Two consecutive calls to BeginTimeRecording will invalidate the first
// returned callback.
TEST_F(TabSwitchTimeRecorderTest, InvalidateCallback) {
  const auto start1 = base::TimeTicks::Now();
  auto callback1 = tab_switch_time_recorder_.BeginTimeRecording(
      start1, false /* has_saved_frames */);

  const auto start2 = base::TimeTicks::Now();
  auto callback2 = tab_switch_time_recorder_.BeginTimeRecording(
      start2, false /* has_saved_frames */);

  // callback1 should be invalid. Running it should not upload anything to
  // histogram.
  const auto end = base::TimeTicks::Now();
  auto presentation_feedback1 = gfx::PresentationFeedback(
      end, end - start1, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback1).Run(presentation_feedback1);
  EXPECT_EQ(GetHistogramSampleSize(true /* has_saved_frames */), 0ULL);
  EXPECT_EQ(GetHistogramSampleSize(false /* has_saved_frames */), 0ULL);

  // callback2 should still be valid.
  auto presentation_feedback2 = gfx::PresentationFeedback(
      end, end - start2, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback2).Run(presentation_feedback2);
  EXPECT_EQ(GetHistogramSampleSize(true /* has_saved_frames */), 0ULL);
  EXPECT_EQ(GetHistogramSampleSize(false /* has_saved_frames */), 1ULL);
}

}  // namespace content
