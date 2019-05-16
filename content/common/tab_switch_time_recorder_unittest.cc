// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "content/common/tab_switch_time_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/presentation_feedback.h"

namespace content {

constexpr char kDurationWithSavedFramesHistogram[] =
    "Browser.Tabs.TotalSwitchDuration.WithSavedFrames";
constexpr char kDurationNoSavedFramesHistogram[] =
    "Browser.Tabs.TotalSwitchDuration.NoSavedFrames";
constexpr char kIncompleteDurationWithSavedFramesHistogram[] =
    "Browser.Tabs.TotalIncompleteSwitchDuration.WithSavedFrames";
constexpr char kIncompleteDurationNoSavedFramesHistogram[] =
    "Browser.Tabs.TotalIncompleteSwitchDuration.NoSavedFrames";
constexpr char kResultWithSavedFramesHistogram[] =
    "Browser.Tabs.TabSwitchResult.WithSavedFrames";
constexpr char kResultNoSavedFramesHistogram[] =
    "Browser.Tabs.TabSwitchResult.NoSavedFrames";
constexpr base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(42);
constexpr base::TimeDelta kOtherDuration =
    base::TimeDelta::FromMilliseconds(4242);

class TabSwitchTimeRecorderTest : public testing::Test {
 protected:
  void SetUp() override {
    // Start with non-zero time.
    scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(123));

    ExpectTotalSamples(kDurationWithSavedFramesHistogram, 0);
    ExpectTotalSamples(kDurationNoSavedFramesHistogram, 0);
  }

  void ExpectTotalSamples(const char* histogram_name, int expected_count) {
    EXPECT_EQ(static_cast<int>(
                  histogram_tester_.GetAllSamples(histogram_name).size()),
              expected_count);
  }

  void ExpectTimeBucketCount(const char* histogram_name,
                             base::TimeDelta value,
                             int count) {
    histogram_tester_.ExpectTimeBucketCount(histogram_name, value, count);
  }

  void ExpectResultBucketCount(const char* histogram_name,
                               TabSwitchTimeRecorder::TabSwitchResult value,
                               int count) {
    histogram_tester_.ExpectBucketCount(histogram_name, value, count);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
      base::test::ScopedTaskEnvironment::NowSource::MAIN_THREAD_MOCK_TIME};
  TabSwitchTimeRecorder tab_switch_time_recorder_;
  base::HistogramTester histogram_tester_;
};

// Time is properly recorded to histogram when we have saved frames and if we
// have a proper matching TabWasShown and callback execution.
TEST_F(TabSwitchTimeRecorderTest, TimeIsRecordedWithSavedFrames) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      true /* has_saved_frames */, start, start);
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectTotalSamples(kDurationWithSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kDurationWithSavedFramesHistogram, kDuration, 1);
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 1);
  ExpectResultBucketCount(kResultWithSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kSuccess, 1);
  ExpectTotalSamples(kIncompleteDurationWithSavedFramesHistogram, 0);

  ExpectTotalSamples(kDurationNoSavedFramesHistogram, 0);
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 0);
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesHistogram, 0);
}

// Time is properly recorded to histogram when we have no saved frame and if we
// have a proper matching TabWasShown and callback execution.
TEST_F(TabSwitchTimeRecorderTest, TimeIsRecordedNoSavedFrame) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */, start, start);
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectTotalSamples(kDurationWithSavedFramesHistogram, 0);
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 0);
  ExpectTotalSamples(kIncompleteDurationWithSavedFramesHistogram, 0);

  ExpectTotalSamples(kDurationNoSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesHistogram, kDuration, 1);
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 1);
  ExpectResultBucketCount(kResultNoSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kSuccess, 1);
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesHistogram, 0);
}

// A failure should be reported if gfx::PresentationFeedback contains the
// kFailure flag.
TEST_F(TabSwitchTimeRecorderTest, PresentationFailureWithSavedFrames) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      true /* has_saved_frames */, start, start);
  std::move(callback).Run(gfx::PresentationFeedback::Failure());

  ExpectTotalSamples(kDurationWithSavedFramesHistogram, 0);
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 1);
  ExpectResultBucketCount(
      kResultWithSavedFramesHistogram,
      TabSwitchTimeRecorder::TabSwitchResult::kPresentationFailure, 1);
  ExpectTotalSamples(kIncompleteDurationWithSavedFramesHistogram, 0);

  ExpectTotalSamples(kDurationNoSavedFramesHistogram, 0);
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 0);
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesHistogram, 0);
}

// A failure should be reported if gfx::PresentationFeedback contains the
// kFailure flag.
TEST_F(TabSwitchTimeRecorderTest, PresentationFailureNoSavedFrames) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */, start, start);
  std::move(callback).Run(gfx::PresentationFeedback::Failure());

  ExpectTotalSamples(kDurationWithSavedFramesHistogram, 0);
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 0);
  ExpectTotalSamples(kIncompleteDurationWithSavedFramesHistogram, 0);

  ExpectTotalSamples(kDurationNoSavedFramesHistogram, 0);
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 1);
  ExpectResultBucketCount(
      kResultNoSavedFramesHistogram,
      TabSwitchTimeRecorder::TabSwitchResult::kPresentationFailure, 1);
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesHistogram, 0);
}

// An incomplete tab switch is reported when no frame is shown before a tab is
// hidden.
TEST_F(TabSwitchTimeRecorderTest, HideBeforePresentFrameWithSavedFrames) {
  const auto start1 = base::TimeTicks::Now();
  auto callback1 = tab_switch_time_recorder_.TabWasShown(
      true /* has_saved_frames */, start1, start1);

  scoped_task_environment_.FastForwardBy(kDuration);
  tab_switch_time_recorder_.TabWasHidden();

  ExpectTotalSamples(kDurationWithSavedFramesHistogram, 0);
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 1);
  ExpectResultBucketCount(kResultWithSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kIncomplete,
                          1);
  ExpectTotalSamples(kIncompleteDurationWithSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kIncompleteDurationWithSavedFramesHistogram, kDuration,
                        1);

  ExpectTotalSamples(kDurationNoSavedFramesHistogram, 0);
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 0);
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesHistogram, 0);

  const auto start2 = base::TimeTicks::Now();
  auto callback2 = tab_switch_time_recorder_.TabWasShown(
      true /* has_saved_frames */, start2, start2);
  const auto end2 = start2 + kOtherDuration;

  auto presentation_feedback = gfx::PresentationFeedback(
      end2, end2 - start2, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback2).Run(presentation_feedback);

  ExpectTotalSamples(kDurationWithSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kDurationWithSavedFramesHistogram, kOtherDuration, 1);
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 2);
  ExpectResultBucketCount(kResultWithSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kIncomplete,
                          1);
  ExpectResultBucketCount(kResultWithSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kSuccess, 1);
  ExpectTotalSamples(kIncompleteDurationWithSavedFramesHistogram, 1);

  ExpectTotalSamples(kDurationNoSavedFramesHistogram, 0);
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 0);
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesHistogram, 0);
}

// An incomplete tab switch is reported when no frame is shown before a tab is
// hidden.
TEST_F(TabSwitchTimeRecorderTest, HideBeforePresentFrameNoSavedFrames) {
  const auto start1 = base::TimeTicks::Now();
  auto callback1 = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */, start1, start1);

  scoped_task_environment_.FastForwardBy(kDuration);
  tab_switch_time_recorder_.TabWasHidden();

  ExpectTotalSamples(kDurationWithSavedFramesHistogram, 0);
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 0);
  ExpectTotalSamples(kIncompleteDurationWithSavedFramesHistogram, 0);

  ExpectTotalSamples(kDurationNoSavedFramesHistogram, 0);
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 1);
  ExpectResultBucketCount(kResultNoSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kIncomplete,
                          1);
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kIncompleteDurationNoSavedFramesHistogram, kDuration,
                        1);

  const auto start2 = base::TimeTicks::Now();
  auto callback2 = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */, start2, start2);
  const auto end2 = start2 + kOtherDuration;

  auto presentation_feedback = gfx::PresentationFeedback(
      end2, end2 - start2, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback2).Run(presentation_feedback);

  ExpectTotalSamples(kDurationWithSavedFramesHistogram, 0);
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 0);
  ExpectTotalSamples(kIncompleteDurationWithSavedFramesHistogram, 0);

  ExpectTotalSamples(kDurationNoSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesHistogram, kOtherDuration, 1);
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 2);
  ExpectResultBucketCount(kResultNoSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kIncomplete,
                          1);
  ExpectResultBucketCount(kResultNoSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kSuccess, 1);
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesHistogram, 1);
}

}  // namespace content
