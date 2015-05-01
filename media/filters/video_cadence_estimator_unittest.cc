// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "media/filters/video_cadence_estimator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// See VideoCadenceEstimator header for more details.
const int kMinimumAcceptableTimeBetweenGlitchesSecs = 8;

// Slows down the given |fps| according to NTSC field reduction standards; see
// http://en.wikipedia.org/wiki/Frame_rate#Digital_video_and_television
static double NTSC(double fps) {
  return fps / 1.001;
}

static base::TimeDelta Interval(double hertz) {
  return base::TimeDelta::FromSecondsD(1.0 / hertz);
}

static void VerifyCadence(VideoCadenceEstimator* estimator,
                          double frame_hertz,
                          double render_hertz,
                          int expected_cadence) {
  SCOPED_TRACE(base::StringPrintf("Checking %.03f fps into %0.03f", frame_hertz,
                                  render_hertz));
  estimator->Reset();
  const base::TimeDelta acceptable_drift = Interval(frame_hertz) / 2;
  const bool cadence_changed = estimator->UpdateCadenceEstimate(
      Interval(render_hertz), Interval(frame_hertz), acceptable_drift);
  EXPECT_EQ(cadence_changed, estimator->has_cadence());
  EXPECT_EQ(!!expected_cadence, estimator->has_cadence());

  // Nothing further to test.
  if (!expected_cadence)
    return;

  // Spot check a few frame indices.
  if (frame_hertz <= render_hertz) {
    EXPECT_EQ(expected_cadence, estimator->GetCadenceForFrame(0));
    EXPECT_EQ(expected_cadence, estimator->GetCadenceForFrame(1));
    EXPECT_EQ(expected_cadence, estimator->GetCadenceForFrame(2));
  } else {
    EXPECT_EQ(1, estimator->GetCadenceForFrame(0));
    EXPECT_EQ(0, estimator->GetCadenceForFrame(1));
    EXPECT_EQ(1, estimator->GetCadenceForFrame(expected_cadence));
    EXPECT_EQ(0, estimator->GetCadenceForFrame(expected_cadence + 1));
  }
}

// Spot check common display and frame rate pairs for correctness.
TEST(VideoCadenceEstimatorTest, CadenceCalculations) {
  VideoCadenceEstimator estimator(
      base::TimeDelta::FromSeconds(kMinimumAcceptableTimeBetweenGlitchesSecs));
  estimator.set_cadence_hysteresis_threshold_for_testing(base::TimeDelta());

  VerifyCadence(&estimator, 24, 60, 0);
  VerifyCadence(&estimator, NTSC(24), 60, 0);
  VerifyCadence(&estimator, 25, 60, 0);
  VerifyCadence(&estimator, NTSC(30), 60, 2);
  VerifyCadence(&estimator, 30, 60, 2);
  VerifyCadence(&estimator, 50, 60, 0);
  VerifyCadence(&estimator, NTSC(60), 60, 1);
  VerifyCadence(&estimator, 120, 60, 2);

  // 50Hz is common in the EU.
  VerifyCadence(&estimator, NTSC(24), 50, 0);
  VerifyCadence(&estimator, 24, 50, 0);
  VerifyCadence(&estimator, NTSC(25), 50, 2);
  VerifyCadence(&estimator, 25, 50, 2);
  VerifyCadence(&estimator, NTSC(30), 50, 0);
  VerifyCadence(&estimator, 30, 50, 0);
  VerifyCadence(&estimator, NTSC(60), 50, 0);
  VerifyCadence(&estimator, 60, 50, 0);

  VerifyCadence(&estimator, 25, NTSC(60), 0);
  VerifyCadence(&estimator, 120, NTSC(60), 0);
  VerifyCadence(&estimator, 1, NTSC(60), 60);
}

TEST(VideoCadenceEstimatorTest, CadenceVariesWithAcceptableDrift) {
  VideoCadenceEstimator estimator(
      base::TimeDelta::FromSeconds(kMinimumAcceptableTimeBetweenGlitchesSecs));
  estimator.set_cadence_hysteresis_threshold_for_testing(base::TimeDelta());

  const base::TimeDelta render_interval = Interval(NTSC(60));
  const base::TimeDelta frame_interval = Interval(120);

  base::TimeDelta acceptable_drift = frame_interval / 2;
  EXPECT_FALSE(estimator.UpdateCadenceEstimate(render_interval, frame_interval,
                                               acceptable_drift));
  EXPECT_FALSE(estimator.has_cadence());

  // Increasing the acceptable drift should be result in more permissive
  // detection of cadence.
  acceptable_drift = render_interval;
  EXPECT_TRUE(estimator.UpdateCadenceEstimate(render_interval, frame_interval,
                                              acceptable_drift));
  EXPECT_TRUE(estimator.has_cadence());
  EXPECT_EQ(2, estimator.get_cadence_for_testing());
}

TEST(VideoCadenceEstimatorTest, CadenceVariesWithAcceptableGlitchTime) {
  scoped_ptr<VideoCadenceEstimator> estimator(new VideoCadenceEstimator(
      base::TimeDelta::FromSeconds(kMinimumAcceptableTimeBetweenGlitchesSecs)));
  estimator->set_cadence_hysteresis_threshold_for_testing(base::TimeDelta());

  const base::TimeDelta render_interval = Interval(NTSC(60));
  const base::TimeDelta frame_interval = Interval(120);
  const base::TimeDelta acceptable_drift = frame_interval / 2;

  EXPECT_FALSE(estimator->UpdateCadenceEstimate(render_interval, frame_interval,
                                                acceptable_drift));
  EXPECT_FALSE(estimator->has_cadence());

  // Decreasing the acceptable glitch time should be result in more permissive
  // detection of cadence.
  estimator.reset(new VideoCadenceEstimator(base::TimeDelta::FromSeconds(
      kMinimumAcceptableTimeBetweenGlitchesSecs / 2)));
  estimator->set_cadence_hysteresis_threshold_for_testing(base::TimeDelta());
  EXPECT_TRUE(estimator->UpdateCadenceEstimate(render_interval, frame_interval,
                                               acceptable_drift));
  EXPECT_TRUE(estimator->has_cadence());
  EXPECT_EQ(2, estimator->get_cadence_for_testing());
}

TEST(VideoCadenceEstimatorTest, CadenceHystersisPreventsOscillation) {
  scoped_ptr<VideoCadenceEstimator> estimator(new VideoCadenceEstimator(
      base::TimeDelta::FromSeconds(kMinimumAcceptableTimeBetweenGlitchesSecs)));

  const base::TimeDelta render_interval = Interval(30);
  const base::TimeDelta frame_interval = Interval(60);
  const base::TimeDelta acceptable_drift = frame_interval / 2;
  estimator->set_cadence_hysteresis_threshold_for_testing(render_interval * 2);

  // Cadence hysteresis should prevent the cadence from taking effect yet.
  EXPECT_FALSE(estimator->UpdateCadenceEstimate(render_interval, frame_interval,
                                                acceptable_drift));
  EXPECT_FALSE(estimator->has_cadence());

  // A second call should exceed cadence hysteresis and take into effect.
  EXPECT_TRUE(estimator->UpdateCadenceEstimate(render_interval, frame_interval,
                                               acceptable_drift));
  EXPECT_TRUE(estimator->has_cadence());

  // One bad interval shouldn't cause cadence to drop
  EXPECT_FALSE(estimator->UpdateCadenceEstimate(
      render_interval, frame_interval * 0.75, acceptable_drift));
  EXPECT_TRUE(estimator->has_cadence());

  // Resumption of cadence should clear bad interval count.
  EXPECT_FALSE(estimator->UpdateCadenceEstimate(render_interval, frame_interval,
                                                acceptable_drift));
  EXPECT_TRUE(estimator->has_cadence());

  // So one more bad interval shouldn't cause cadence to drop
  EXPECT_FALSE(estimator->UpdateCadenceEstimate(
      render_interval, frame_interval * 0.75, acceptable_drift));
  EXPECT_TRUE(estimator->has_cadence());

  // Two bad intervals should.
  EXPECT_TRUE(estimator->UpdateCadenceEstimate(
      render_interval, frame_interval * 0.75, acceptable_drift));
  EXPECT_FALSE(estimator->has_cadence());
}

}  // namespace media
