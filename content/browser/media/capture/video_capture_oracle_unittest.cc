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

// Tests that VideoCaptureOracle is enforcing the requirement that
// successfully captured frames are delivered in order.  Otherwise, downstream
// consumers could be tripped-up by out-of-order frames or frame timestamps.
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
    ASSERT_TRUE(oracle.CompleteCapture(last_frame_number, true, &ignored));
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
      ASSERT_TRUE(
          oracle.CompleteCapture(last_frame_number - j, true, &ignored));
    }
  }

  // Pipelined scenario with successful out-of-order delivery attempts
  // rejected.
  for (int i = 0; i < 50; ++i) {
    const int num_in_flight = 1 + i % 3;
    for (int j = 0; j < num_in_flight; ++j) {
      t += event_increment;
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate,
          damage_rect, t));
      last_frame_number = oracle.RecordCapture();
    }
    ASSERT_TRUE(oracle.CompleteCapture(last_frame_number, true, &ignored));
    for (int j = 1; j < num_in_flight; ++j) {
      ASSERT_FALSE(
          oracle.CompleteCapture(last_frame_number - j, true, &ignored));
    }
  }

  // Pipelined scenario with successful delivery attempts accepted after an
  // unsuccessful out of order delivery attempt.
  for (int i = 0; i < 50; ++i) {
    const int num_in_flight = 1 + i % 3;
    for (int j = 0; j < num_in_flight; ++j) {
      t += event_increment;
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate,
          damage_rect, t));
      last_frame_number = oracle.RecordCapture();
    }
    // Report the last frame as an out of order failure.
    ASSERT_FALSE(oracle.CompleteCapture(last_frame_number, false, &ignored));
    for (int j = 1; j < num_in_flight - 1; ++j) {
      ASSERT_TRUE(
          oracle.CompleteCapture(last_frame_number - j, true, &ignored));
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
    if (!oracle_says_sample) {
      ASSERT_EQ(base::TimeDelta(), oracle.estimated_frame_duration());
      continue;
    }
    ASSERT_LT(base::TimeDelta(), oracle.estimated_frame_duration());

    const int frame_number = oracle.RecordCapture();

    base::TimeTicks frame_timestamp;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &frame_timestamp));
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

// Tests that VideoCaptureOracle prevents timer polling from initiating
// simultaneous captures.
TEST(VideoCaptureOracleTest, SamplesOnlyOneOverdueFrameAtATime) {
  const base::TimeDelta min_capture_period =
      base::TimeDelta::FromSeconds(1) / 30;
  const base::TimeDelta vsync_interval =
      base::TimeDelta::FromSeconds(1) / 60;
  const base::TimeDelta timer_interval = base::TimeDelta::FromMilliseconds(
      VideoCaptureOracle::kMinTimerPollPeriodMillis);

  VideoCaptureOracle oracle(min_capture_period);

  // Have the oracle observe some compositor events.  Simulate that each capture
  // completes successfully.
  base::TimeTicks t = InitialTestTimeTicks();
  base::TimeTicks ignored;
  bool did_complete_a_capture = false;
  for (int i = 0; i < 10; ++i) {
    t += vsync_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t)) {
      ASSERT_TRUE(
          oracle.CompleteCapture(oracle.RecordCapture(), true, &ignored));
      did_complete_a_capture = true;
    }
  }
  ASSERT_TRUE(did_complete_a_capture);

  // Start one more compositor-based capture, but do not notify of completion
  // yet.
  for (int i = 0; i <= 10; ++i) {
    ASSERT_GT(10, i) << "BUG: Seems like it'll never happen!";
    t += vsync_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t)) {
      break;
    }
  }
  int frame_number = oracle.RecordCapture();

  // Stop providing the compositor events and start providing timer polling
  // events.  No overdue samplings should be recommended because of the
  // not-yet-complete compositor-based capture.
  for (int i = 0; i < 10; ++i) {
    t += timer_interval;
    ASSERT_FALSE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kTimerPoll, gfx::Rect(), t));
  }

  // Now, complete the oustanding compositor-based capture and continue
  // providing timer polling events.  The oracle should start recommending
  // sampling again.
  ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  did_complete_a_capture = false;
  for (int i = 0; i < 10; ++i) {
    t += timer_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kTimerPoll, gfx::Rect(), t)) {
      ASSERT_TRUE(
          oracle.CompleteCapture(oracle.RecordCapture(), true, &ignored));
      did_complete_a_capture = true;
    }
  }
  ASSERT_TRUE(did_complete_a_capture);

  // Start one more timer-based capture, but do not notify of completion yet.
  for (int i = 0; i <= 10; ++i) {
    ASSERT_GT(10, i) << "BUG: Seems like it'll never happen!";
    t += timer_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kTimerPoll, gfx::Rect(), t)) {
      break;
    }
  }
  frame_number = oracle.RecordCapture();

  // Confirm that the oracle does not recommend sampling until the outstanding
  // timer-based capture completes.
  for (int i = 0; i < 10; ++i) {
    t += timer_interval;
    ASSERT_FALSE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kTimerPoll, gfx::Rect(), t));
  }
  ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  for (int i = 0; i <= 10; ++i) {
    ASSERT_GT(10, i) << "BUG: Seems like it'll never happen!";
    t += timer_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kTimerPoll, gfx::Rect(), t)) {
      break;
    }
  }
}

}  // namespace content
