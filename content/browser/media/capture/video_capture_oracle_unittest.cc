// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/video_capture_oracle.h"

#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

base::TimeTicks InitialTestTimeTicks() {
  return base::TimeTicks() + base::TimeDelta::FromSeconds(1);
}

}  // namespace

// Tests that VideoCaptureOracle filters out events whose timestamps are
// decreasing.
TEST(VideoCaptureOracleTest, EnforcesEventTimeMonotonicity) {
  const base::TimeDelta min_capture_period =
      base::TimeDelta::FromSeconds(1) / 30;
  const gfx::Rect damage_rect(0, 0, 1280, 720);
  const base::TimeDelta event_increment = min_capture_period * 2;

  VideoCaptureOracle oracle(min_capture_period);

  base::TimeTicks t = InitialTestTimeTicks();
  for (int i = 0; i < 10; ++i) {
    t += event_increment;
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate,
        damage_rect, t));
  }

  base::TimeTicks furthest_event_time = t;
  for (int i = 0; i < 10; ++i) {
    t -= event_increment;
    ASSERT_FALSE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate,
        damage_rect, t));
  }

  t = furthest_event_time;
  for (int i = 0; i < 10; ++i) {
    t += event_increment;
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate,
        damage_rect, t));
  }
}

// Tests that VideoCaptureOracle is enforcing the requirement that captured
// frames are delivered in order.  Otherwise, downstream consumers could be
// tripped-up by out-of-order frames or frame timestamps.
TEST(VideoCaptureOracleTest, EnforcesFramesDeliveredInOrder) {
  const base::TimeDelta min_capture_period =
      base::TimeDelta::FromSeconds(1) / 30;
  const gfx::Rect damage_rect(0, 0, 1280, 720);
  const base::TimeDelta event_increment = min_capture_period * 2;

  VideoCaptureOracle oracle(min_capture_period);

  // Most basic scenario: Frames delivered one at a time, with no additional
  // captures in-between deliveries.
  base::TimeTicks t = InitialTestTimeTicks();
  int last_frame_number;
  base::TimeTicks ignored;
  for (int i = 0; i < 10; ++i) {
    t += event_increment;
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate,
        damage_rect, t));
    last_frame_number = oracle.RecordCapture();
    ASSERT_TRUE(oracle.CompleteCapture(last_frame_number, &ignored));
  }

  // Basic pipelined scenario: More than one frame in-flight at delivery points.
  for (int i = 0; i < 50; ++i) {
    const int num_in_flight = 1 + i % 3;
    for (int j = 0; j < num_in_flight; ++j) {
      t += event_increment;
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate,
          damage_rect, t));
      last_frame_number = oracle.RecordCapture();
    }
    for (int j = num_in_flight - 1; j >= 0; --j) {
      ASSERT_TRUE(oracle.CompleteCapture(last_frame_number - j, &ignored));
    }
  }

  // Pipelined scenario with out-of-order delivery attempts rejected.
  for (int i = 0; i < 50; ++i) {
    const int num_in_flight = 1 + i % 3;
    for (int j = 0; j < num_in_flight; ++j) {
      t += event_increment;
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate,
          damage_rect, t));
      last_frame_number = oracle.RecordCapture();
    }
    ASSERT_TRUE(oracle.CompleteCapture(last_frame_number, &ignored));
    for (int j = 1; j < num_in_flight; ++j) {
      ASSERT_FALSE(oracle.CompleteCapture(last_frame_number - j, &ignored));
    }
  }
}

// Tests that VideoCaptureOracle transitions between using its two samplers in a
// way that does not introduce severe jank, pauses, etc.
TEST(VideoCaptureOracleTest, TransitionsSmoothlyBetweenSamplers) {
  const base::TimeDelta min_capture_period =
      base::TimeDelta::FromSeconds(1) / 30;
  const gfx::Rect animation_damage_rect(0, 0, 1280, 720);
  const base::TimeDelta event_increment = min_capture_period * 2;

  VideoCaptureOracle oracle(min_capture_period);

  // Run sequences of animation events and non-animation events through the
  // oracle.  As the oracle transitions between each sampler, make sure the
  // frame timestamps won't trip-up downstream consumers.
  base::TimeTicks t = InitialTestTimeTicks();
  base::TimeTicks last_frame_timestamp;
  for (int i = 0; i < 1000; ++i) {
    t += event_increment;

    // For every 100 events, provide 50 that will cause the
    // AnimatedContentSampler to lock-in, followed by 50 that will cause it to
    // lock-out (i.e., the oracle will use the SmoothEventSampler instead).
    const bool provide_animated_content_event =
        (i % 100) >= 25 && (i % 100) < 75;

    // Only the few events that trigger the lock-out transition should be
    // dropped, because the AnimatedContentSampler doesn't yet realize the
    // animation ended.  Otherwise, the oracle should always decide to sample
    // because one of its samplers says to.
    const bool require_oracle_says_sample = (i % 100) < 75 || (i % 100) >= 78;
    const bool oracle_says_sample = oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate,
        provide_animated_content_event ? animation_damage_rect : gfx::Rect(),
        t);
    if (require_oracle_says_sample)
      ASSERT_TRUE(oracle_says_sample);
    if (!oracle_says_sample)
      continue;

    const int frame_number = oracle.RecordCapture();

    base::TimeTicks frame_timestamp;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, &frame_timestamp));
    ASSERT_FALSE(frame_timestamp.is_null());
    if (!last_frame_timestamp.is_null()) {
      const base::TimeDelta delta = frame_timestamp - last_frame_timestamp;
      EXPECT_LE(event_increment.InMicroseconds(), delta.InMicroseconds());
      // Right after the AnimatedContentSampler lock-out transition, there were
      // a few frames dropped, so allow a gap in the timestamps.  Otherwise, the
      // delta between frame timestamps should never be more than 2X the
      // |event_increment|.
      const base::TimeDelta max_acceptable_delta = (i % 100) == 78 ?
          event_increment * 5 : event_increment * 2;
      EXPECT_GE(max_acceptable_delta.InMicroseconds(), delta.InMicroseconds());
    }
    last_frame_timestamp = frame_timestamp;
  }
}

}  // namespace content
