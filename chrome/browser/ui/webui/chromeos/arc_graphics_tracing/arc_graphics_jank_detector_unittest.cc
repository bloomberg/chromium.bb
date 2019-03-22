// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/arc_graphics_tracing/arc_graphics_jank_detector.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class ArcGraphicsJankDetectorTest : public testing::Test {
 public:
  ArcGraphicsJankDetectorTest() = default;
  ~ArcGraphicsJankDetectorTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcGraphicsJankDetectorTest);
};

TEST_F(ArcGraphicsJankDetectorTest, Generic) {
  int jank_count = 0;
  ArcGraphicsJankDetector detector(base::BindRepeating(
      [](int* out_jank_count) { *out_jank_count += 1; }, &jank_count));

  base::Time now = base::Time::Now();

  const base::TimeDelta interval_fast =
      ArcGraphicsJankDetector::kPauseDetectionThreshold / 6;
  const base::TimeDelta interval_normal =
      ArcGraphicsJankDetector::kPauseDetectionThreshold / 4;
  const base::TimeDelta interval_slow =
      ArcGraphicsJankDetector::kPauseDetectionThreshold / 2;

  EXPECT_EQ(0, jank_count);
  for (int i = 0; i < ArcGraphicsJankDetector::kWarmUpSamples; ++i) {
    EXPECT_EQ(ArcGraphicsJankDetector::Stage::kWarmUp, detector.stage());
    now += interval_normal;
    detector.OnSample(now);
  }

  EXPECT_EQ(0, jank_count);

  for (size_t i = 0; i < ArcGraphicsJankDetector::kSamplesForRateDetection;
       ++i) {
    EXPECT_EQ(ArcGraphicsJankDetector::Stage::kRateDetection, detector.stage());
    if (i < ArcGraphicsJankDetector::kSamplesForRateDetection / 4) {
      // Slow samples.
      now += interval_slow;
    } else if (i < ArcGraphicsJankDetector::kSamplesForRateDetection / 2) {
      // Fast samples.
      now += interval_fast;
    } else {
      // Normal samples.
      now += interval_normal;
    }
    detector.OnSample(now);
  }

  EXPECT_EQ(ArcGraphicsJankDetector::Stage::kActive, detector.stage());
  // Prevailing period should be returned.
  EXPECT_EQ(interval_normal, detector.period());
  EXPECT_EQ(0, jank_count);

  now += interval_normal;
  detector.OnSample(now);
  EXPECT_EQ(ArcGraphicsJankDetector::Stage::kActive, detector.stage());
  EXPECT_EQ(0, jank_count);

  now += interval_fast;
  detector.OnSample(now);
  EXPECT_EQ(ArcGraphicsJankDetector::Stage::kActive, detector.stage());
  EXPECT_EQ(0, jank_count);

  // Simulate jank
  now += interval_slow;
  detector.OnSample(now);
  EXPECT_EQ(ArcGraphicsJankDetector::Stage::kActive, detector.stage());
  EXPECT_EQ(1, jank_count);

  now += interval_normal;
  detector.OnSample(now);
  EXPECT_EQ(ArcGraphicsJankDetector::Stage::kActive, detector.stage());
  EXPECT_EQ(1, jank_count);

  // Simulate pause, detector should switch to warm-up stage, no jank detection
  // should be reported.
  now += ArcGraphicsJankDetector::kPauseDetectionThreshold;
  detector.OnSample(now);
  EXPECT_EQ(ArcGraphicsJankDetector::Stage::kWarmUp, detector.stage());
  EXPECT_EQ(1, jank_count);
}

}  // namespace chromeos
