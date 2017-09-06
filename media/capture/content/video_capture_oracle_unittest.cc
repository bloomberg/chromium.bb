// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/content/video_capture_oracle.h"

#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

base::TimeTicks InitialTestTimeTicks() {
  return base::TimeTicks() + base::TimeDelta::FromSeconds(1);
}

base::TimeDelta Get30HzPeriod() {
  return base::TimeDelta::FromSeconds(1) / 30;
}

gfx::Size Get1080pSize() {
  return gfx::Size(1920, 1080);
}

gfx::Size Get720pSize() {
  return gfx::Size(1280, 720);
}

gfx::Size Get360pSize() {
  return gfx::Size(640, 360);
}

gfx::Size GetSmallestNonEmptySize() {
  return gfx::Size(2, 2);
}

}  // namespace

// Tests that VideoCaptureOracle filters out events whose timestamps are
// decreasing.
TEST(VideoCaptureOracleTest, EnforcesEventTimeMonotonicity) {
  const gfx::Rect damage_rect(Get720pSize());
  const base::TimeDelta event_increment = Get30HzPeriod() * 2;

  VideoCaptureOracle oracle(false);
  oracle.SetMinCapturePeriod(Get30HzPeriod());
  oracle.SetCaptureSizeConstraints(Get720pSize(), Get720pSize(), false);

  base::TimeTicks t = InitialTestTimeTicks();
  for (int i = 0; i < 10; ++i) {
    t += event_increment;
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, damage_rect, t));
  }

  base::TimeTicks furthest_event_time = t;
  for (int i = 0; i < 10; ++i) {
    t -= event_increment;
    ASSERT_FALSE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, damage_rect, t));
  }

  t = furthest_event_time;
  for (int i = 0; i < 10; ++i) {
    t += event_increment;
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, damage_rect, t));
  }
}

// Tests that VideoCaptureOracle is enforcing the requirement that
// successfully captured frames are delivered in order.  Otherwise, downstream
// consumers could be tripped-up by out-of-order frames or frame timestamps.
TEST(VideoCaptureOracleTest, EnforcesFramesDeliveredInOrder) {
  const gfx::Rect damage_rect(Get720pSize());
  const base::TimeDelta event_increment = Get30HzPeriod() * 2;

  VideoCaptureOracle oracle(false);
  oracle.SetMinCapturePeriod(Get30HzPeriod());
  oracle.SetCaptureSizeConstraints(Get720pSize(), Get720pSize(), false);

  // Most basic scenario: Frames delivered one at a time, with no additional
  // captures in-between deliveries.
  base::TimeTicks t = InitialTestTimeTicks();
  int last_frame_number;
  base::TimeTicks ignored;
  for (int i = 0; i < 10; ++i) {
    t += event_increment;
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, damage_rect, t));
    last_frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.0);
    ASSERT_TRUE(oracle.CompleteCapture(last_frame_number, true, &ignored));
  }

  // Basic pipelined scenario: More than one frame in-flight at delivery points.
  for (int i = 0; i < 50; ++i) {
    const int num_in_flight = 1 + i % 3;
    for (int j = 0; j < num_in_flight; ++j) {
      t += event_increment;
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate, damage_rect, t));
      last_frame_number = oracle.next_frame_number();
      oracle.RecordCapture(0.0);
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
          VideoCaptureOracle::kCompositorUpdate, damage_rect, t));
      last_frame_number = oracle.next_frame_number();
      oracle.RecordCapture(0.0);
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
          VideoCaptureOracle::kCompositorUpdate, damage_rect, t));
      last_frame_number = oracle.next_frame_number();
      oracle.RecordCapture(0.0);
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
  const gfx::Rect animation_damage_rect(Get720pSize());
  const base::TimeDelta event_increment = Get30HzPeriod() * 2;

  VideoCaptureOracle oracle(false);
  oracle.SetMinCapturePeriod(Get30HzPeriod());
  oracle.SetCaptureSizeConstraints(Get720pSize(), Get720pSize(), false);

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

    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.0);

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
      const base::TimeDelta max_acceptable_delta =
          (i % 100) == 78 ? event_increment * 5 : event_increment * 2;
      EXPECT_GE(max_acceptable_delta.InMicroseconds(), delta.InMicroseconds());
    }
    last_frame_timestamp = frame_timestamp;
  }
}

// Tests that VideoCaptureOracle prevents refresh request events from initiating
// simultaneous captures.
TEST(VideoCaptureOracleTest, SamplesAtCorrectTimesAroundRefreshRequests) {
  const base::TimeDelta vsync_interval = base::TimeDelta::FromSeconds(1) / 60;
  const base::TimeDelta refresh_interval =
      base::TimeDelta::FromMilliseconds(125);  // 8 FPS

  VideoCaptureOracle oracle(false);
  oracle.SetMinCapturePeriod(Get30HzPeriod());
  oracle.SetCaptureSizeConstraints(Get720pSize(), Get720pSize(), false);

  // Have the oracle observe some compositor events.  Simulate that each capture
  // completes successfully.
  base::TimeTicks t = InitialTestTimeTicks();
  base::TimeTicks ignored;
  bool did_complete_a_capture = false;
  for (int i = 0; i < 10; ++i) {
    t += vsync_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t)) {
      const int frame_number = oracle.next_frame_number();
      oracle.RecordCapture(0.0);
      ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
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
  int frame_number = oracle.next_frame_number();
  oracle.RecordCapture(0.0);

  // Stop providing the compositor events and start providing refresh request
  // events.  No overdue samplings should be recommended because of the
  // not-yet-complete compositor-based capture.
  for (int i = 0; i < 10; ++i) {
    t += refresh_interval;
    ASSERT_FALSE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kPassiveRefreshRequest, gfx::Rect(), t));
  }

  // Now, complete the oustanding compositor-based capture and continue
  // providing refresh request events.  The oracle should start recommending
  // sampling again.
  ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  did_complete_a_capture = false;
  for (int i = 0; i < 10; ++i) {
    t += refresh_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kPassiveRefreshRequest, gfx::Rect(), t)) {
      const int frame_number = oracle.next_frame_number();
      oracle.RecordCapture(0.0);
      ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
      did_complete_a_capture = true;
    }
  }
  ASSERT_TRUE(did_complete_a_capture);

  // Start one more "refresh" capture, but do not notify of completion yet.
  for (int i = 0; i <= 10; ++i) {
    ASSERT_GT(10, i) << "BUG: Seems like it'll never happen!";
    t += refresh_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kPassiveRefreshRequest, gfx::Rect(), t)) {
      break;
    }
  }
  frame_number = oracle.next_frame_number();
  oracle.RecordCapture(0.0);

  // Confirm that the oracle does not recommend sampling until the outstanding
  // "refresh" capture completes.
  for (int i = 0; i < 10; ++i) {
    t += refresh_interval;
    ASSERT_FALSE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kPassiveRefreshRequest, gfx::Rect(), t));
  }
  ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  for (int i = 0; i <= 10; ++i) {
    ASSERT_GT(10, i) << "BUG: Seems like it'll never happen!";
    t += refresh_interval;
    if (oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kPassiveRefreshRequest, gfx::Rect(), t)) {
      break;
    }
  }
}

// Tests that VideoCaptureOracle does not rapidly change proposed capture sizes,
// to allow both the source content and the rest of the end-to-end system to
// stabilize.
TEST(VideoCaptureOracleTest, DoesNotRapidlyChangeCaptureSize) {
  VideoCaptureOracle oracle(true);
  oracle.SetMinCapturePeriod(Get30HzPeriod());
  oracle.SetCaptureSizeConstraints(GetSmallestNonEmptySize(), Get720pSize(),
                                   false);
  oracle.SetSourceSize(Get1080pSize());

  // Run 30 seconds of frame captures without any source size changes.
  base::TimeTicks t = InitialTestTimeTicks();
  const base::TimeDelta event_increment = Get30HzPeriod() * 2;
  base::TimeTicks end_t = t + base::TimeDelta::FromSeconds(30);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(Get720pSize(), oracle.capture_size());
    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.0);
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  }

  // Now run 30 seconds of frame captures with lots of random source size
  // changes.  Check that there was no more than one size change per second.
  gfx::Size source_size = oracle.capture_size();
  base::TimeTicks time_of_last_size_change = InitialTestTimeTicks();
  gfx::Size last_capture_size = oracle.capture_size();
  end_t = t + base::TimeDelta::FromSeconds(30);
  for (; t < end_t; t += event_increment) {
    // Change the source size every frame to a random non-empty size.
    const gfx::Size last_source_size = source_size;
    source_size.SetSize(((last_source_size.width() * 11 + 12345) % 1280) + 1,
                        ((last_source_size.height() * 11 + 12345) % 720) + 1);
    ASSERT_NE(last_source_size, source_size);
    oracle.SetSourceSize(source_size);

    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));

    if (oracle.capture_size() != last_capture_size) {
      ASSERT_GE(t - time_of_last_size_change, base::TimeDelta::FromSeconds(1));
      time_of_last_size_change = t;
      last_capture_size = oracle.capture_size();
    }

    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.0);
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  }
}

// Tests that un-sampled compositor update event will fail the next passive
// refresh request, forcing an active refresh.
TEST(VideoCaptureOracleTest, EnforceActiveRefreshForUnsampledCompositorUpdate) {
  const gfx::Rect damage_rect(Get720pSize());
  const base::TimeDelta event_increment = Get30HzPeriod() * 2;
  const base::TimeDelta short_event_increment = Get30HzPeriod() / 4;

  VideoCaptureOracle oracle(false);
  oracle.SetMinCapturePeriod(Get30HzPeriod());
  oracle.SetCaptureSizeConstraints(Get720pSize(), Get720pSize(), false);

  base::TimeTicks t = InitialTestTimeTicks();
  int last_frame_number;
  base::TimeTicks ignored;

  // CompositorUpdate is sampled normally.
  t += event_increment;
  ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
      VideoCaptureOracle::kCompositorUpdate, damage_rect, t));
  last_frame_number = oracle.next_frame_number();
  oracle.RecordCapture(0.0);
  ASSERT_TRUE(oracle.CompleteCapture(last_frame_number, true, &ignored));

  // Next CompositorUpdate comes too soon and won't be sampled.
  t += short_event_increment;
  ASSERT_FALSE(oracle.ObserveEventAndDecideCapture(
      VideoCaptureOracle::kCompositorUpdate, damage_rect, t));

  // Then the next valid PassiveRefreshRequest will fail to enforce an
  // ActiveRefreshRequest to capture the updated content.
  t += event_increment;
  ASSERT_FALSE(oracle.ObserveEventAndDecideCapture(
      VideoCaptureOracle::kPassiveRefreshRequest, damage_rect, t));
  ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
      VideoCaptureOracle::kActiveRefreshRequest, damage_rect, t));
}

namespace {

// Tests that VideoCaptureOracle can auto-throttle by stepping the capture size
// up or down.  When |is_content_animating| is false, there is more
// aggressiveness expected in the timing of stepping upwards.  If
// |with_consumer_feedback| is false, only buffer pool utilization varies and no
// consumer feedback is provided.  If |with_consumer_feedback| is true, the
// buffer pool utilization is held constant at 25%, and the consumer utilization
// feedback varies.
void RunAutoThrottleTest(bool is_content_animating,
                         bool with_consumer_feedback) {
  SCOPED_TRACE(::testing::Message()
               << "RunAutoThrottleTest("
               << "(is_content_animating=" << is_content_animating
               << ", with_consumer_feedback=" << with_consumer_feedback << ")");

  VideoCaptureOracle oracle(true);
  oracle.SetMinCapturePeriod(Get30HzPeriod());
  oracle.SetCaptureSizeConstraints(GetSmallestNonEmptySize(), Get720pSize(),
                                   false);
  oracle.SetSourceSize(Get1080pSize());

  // Run 10 seconds of frame captures with 90% utilization expect no capture
  // size changes.
  base::TimeTicks t = InitialTestTimeTicks();
  base::TimeTicks time_of_last_size_change = t;
  const base::TimeDelta event_increment = Get30HzPeriod() * 2;
  base::TimeTicks end_t = t + base::TimeDelta::FromSeconds(10);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate,
        is_content_animating ? gfx::Rect(Get720pSize()) : gfx::Rect(), t));
    ASSERT_EQ(Get720pSize(), oracle.capture_size());
    const double utilization = 0.9;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(with_consumer_feedback ? 0.25 : utilization);
    base::TimeTicks ignored;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
    if (with_consumer_feedback)
      oracle.RecordConsumerFeedback(frame_number, utilization);
  }

  // Cause two downward steppings in resolution.  First, indicate overload
  // until the resolution steps down.  Then, indicate a 90% utilization and
  // expect the resolution to remain constant.  Repeat.
  for (int i = 0; i < 2; ++i) {
    const gfx::Size starting_size = oracle.capture_size();
    SCOPED_TRACE(::testing::Message() << "Stepping down from "
                                      << starting_size.ToString()
                                      << ", i=" << i);

    gfx::Size stepped_down_size;
    end_t = t + base::TimeDelta::FromSeconds(10);
    for (; t < end_t; t += event_increment) {
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate,
          is_content_animating ? gfx::Rect(Get720pSize()) : gfx::Rect(), t));

      if (stepped_down_size.IsEmpty()) {
        if (oracle.capture_size() != starting_size) {
          time_of_last_size_change = t;
          stepped_down_size = oracle.capture_size();
          ASSERT_GT(starting_size.width(), stepped_down_size.width());
          ASSERT_GT(starting_size.height(), stepped_down_size.height());
        }
      } else {
        ASSERT_EQ(stepped_down_size, oracle.capture_size());
      }

      const double utilization = stepped_down_size.IsEmpty() ? 1.5 : 0.9;
      const int frame_number = oracle.next_frame_number();
      oracle.RecordCapture(with_consumer_feedback ? 0.25 : utilization);
      base::TimeTicks ignored;
      ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
      if (with_consumer_feedback)
        oracle.RecordConsumerFeedback(frame_number, utilization);
    }
  }

  // Now, cause two upward steppings in resolution.  First, indicate
  // under-utilization until the resolution steps up.  Then, indicate a 90%
  // utilization and expect the resolution to remain constant.  Repeat.
  for (int i = 0; i < 2; ++i) {
    const gfx::Size starting_size = oracle.capture_size();
    SCOPED_TRACE(::testing::Message() << "Stepping up from "
                                      << starting_size.ToString()
                                      << ", i=" << i);

    gfx::Size stepped_up_size;
    end_t = t + base::TimeDelta::FromSeconds(is_content_animating ? 90 : 10);
    for (; t < end_t; t += event_increment) {
      ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
          VideoCaptureOracle::kCompositorUpdate,
          is_content_animating ? gfx::Rect(Get720pSize()) : gfx::Rect(), t));

      if (stepped_up_size.IsEmpty()) {
        if (oracle.capture_size() != starting_size) {
          // When content is animating, a much longer amount of time must pass
          // before the capture size will step up.
          ASSERT_LT(base::TimeDelta::FromSeconds(is_content_animating ? 15 : 1),
                    t - time_of_last_size_change);
          time_of_last_size_change = t;
          stepped_up_size = oracle.capture_size();
          ASSERT_LT(starting_size.width(), stepped_up_size.width());
          ASSERT_LT(starting_size.height(), stepped_up_size.height());
        }
      } else {
        ASSERT_EQ(stepped_up_size, oracle.capture_size());
      }

      const double utilization = stepped_up_size.IsEmpty() ? 0.0 : 0.9;
      const int frame_number = oracle.next_frame_number();
      oracle.RecordCapture(with_consumer_feedback ? 0.25 : utilization);
      base::TimeTicks ignored;
      ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
      if (with_consumer_feedback)
        oracle.RecordConsumerFeedback(frame_number, utilization);
    }
  }
}

}  // namespace

// Tests that VideoCaptureOracle can auto-throttle by stepping the capture size
// up or down, using utilization feedback signals from either the buffer pool or
// the consumer, and with slightly different behavior depending on whether
// content is animating.
TEST(VideoCaptureOracleTest, AutoThrottlesBasedOnUtilizationFeedback) {
  RunAutoThrottleTest(false, false);
  RunAutoThrottleTest(false, true);
  RunAutoThrottleTest(true, false);
  RunAutoThrottleTest(true, true);
}

// Tests that, while content is animating, VideoCaptureOracle can make frequent
// capture size increases only just after the source size has changed.
// Otherwise, capture size increases should only be made cautiously, after a
// long "proving period of under-utilization" has elapsed.
TEST(VideoCaptureOracleTest, IncreasesFrequentlyOnlyAfterSourceSizeChange) {
  VideoCaptureOracle oracle(true);
  oracle.SetMinCapturePeriod(Get30HzPeriod());
  oracle.SetCaptureSizeConstraints(GetSmallestNonEmptySize(), Get720pSize(),
                                   false);

  // Start out the source size at 360p, so there is room to grow to the 720p
  // maximum.
  oracle.SetSourceSize(Get360pSize());

  // Run 10 seconds of frame captures with under-utilization to represent a
  // machine that can do more, but won't because the source size is small.
  base::TimeTicks t = InitialTestTimeTicks();
  const base::TimeDelta event_increment = Get30HzPeriod() * 2;
  base::TimeTicks end_t = t + base::TimeDelta::FromSeconds(10);
  for (; t < end_t; t += event_increment) {
    if (!oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(Get360pSize()),
            t)) {
      continue;
    }
    ASSERT_EQ(Get360pSize(), oracle.capture_size());
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.25);
    base::TimeTicks ignored;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  }

  // Now, set the source size to 720p, continuing to report under-utilization,
  // and expect the capture size increases to reach a full 720p within 15
  // seconds.
  oracle.SetSourceSize(Get720pSize());
  gfx::Size last_capture_size = oracle.capture_size();
  end_t = t + base::TimeDelta::FromSeconds(15);
  for (; t < end_t; t += event_increment) {
    if (!oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(Get720pSize()),
            t)) {
      continue;
    }
    ASSERT_LE(last_capture_size.width(), oracle.capture_size().width());
    ASSERT_LE(last_capture_size.height(), oracle.capture_size().height());
    last_capture_size = oracle.capture_size();
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.25);
    base::TimeTicks ignored;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  }
  ASSERT_EQ(Get720pSize(), oracle.capture_size());

  // Now, change the source size again, but report over-utilization so the
  // capture size will decrease.  Once it decreases one step, report 90%
  // utilization to achieve a steady-state.
  oracle.SetSourceSize(Get1080pSize());
  gfx::Size stepped_down_size;
  end_t = t + base::TimeDelta::FromSeconds(10);
  for (; t < end_t; t += event_increment) {
    if (!oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(Get1080pSize()),
            t)) {
      continue;
    }

    if (stepped_down_size.IsEmpty()) {
      if (oracle.capture_size() != Get720pSize()) {
        stepped_down_size = oracle.capture_size();
        ASSERT_GT(Get720pSize().width(), stepped_down_size.width());
        ASSERT_GT(Get720pSize().height(), stepped_down_size.height());
      }
    } else {
      ASSERT_EQ(stepped_down_size, oracle.capture_size());
    }

    const double utilization = stepped_down_size.IsEmpty() ? 1.5 : 0.9;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(utilization);
    base::TimeTicks ignored;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  }
  ASSERT_FALSE(stepped_down_size.IsEmpty());

  // Now, if we report under-utilization again (without any source size change),
  // there should be a long "proving period" before there is any increase in
  // capture size made by the oracle.
  const base::TimeTicks proving_period_end_time =
      t + base::TimeDelta::FromSeconds(15);
  gfx::Size stepped_up_size;
  end_t = t + base::TimeDelta::FromSeconds(60);
  for (; t < end_t; t += event_increment) {
    if (!oracle.ObserveEventAndDecideCapture(
            VideoCaptureOracle::kCompositorUpdate, gfx::Rect(Get1080pSize()),
            t)) {
      continue;
    }

    if (stepped_up_size.IsEmpty()) {
      if (oracle.capture_size() != stepped_down_size) {
        ASSERT_LT(proving_period_end_time, t);
        stepped_up_size = oracle.capture_size();
        ASSERT_LT(stepped_down_size.width(), stepped_up_size.width());
        ASSERT_LT(stepped_down_size.height(), stepped_up_size.height());
      }
    } else {
      ASSERT_EQ(stepped_up_size, oracle.capture_size());
    }

    const double utilization = stepped_up_size.IsEmpty() ? 0.25 : 0.9;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(utilization);
    base::TimeTicks ignored;
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  }
  ASSERT_FALSE(stepped_up_size.IsEmpty());
}

// Tests that VideoCaptureOracle does not change the capture size if
// auto-throttling is enabled when using a fixed resolution policy.
TEST(VideoCaptureOracleTest, DoesNotAutoThrottleWhenResolutionIsFixed) {
  VideoCaptureOracle oracle(true);
  oracle.SetMinCapturePeriod(Get30HzPeriod());
  oracle.SetCaptureSizeConstraints(Get720pSize(), Get720pSize(), false);
  oracle.SetSourceSize(Get1080pSize());

  // Run 10 seconds of frame captures with 90% utilization expect no capture
  // size changes.
  base::TimeTicks t = InitialTestTimeTicks();
  const base::TimeDelta event_increment = Get30HzPeriod() * 2;
  base::TimeTicks end_t = t + base::TimeDelta::FromSeconds(10);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(Get720pSize(), oracle.capture_size());
    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(0.9);
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  }

  // Now run 10 seconds with overload indicated.  Still, expect no capture size
  // changes.
  end_t = t + base::TimeDelta::FromSeconds(10);
  for (; t < end_t; t += event_increment) {
    ASSERT_TRUE(oracle.ObserveEventAndDecideCapture(
        VideoCaptureOracle::kCompositorUpdate, gfx::Rect(), t));
    ASSERT_EQ(Get720pSize(), oracle.capture_size());
    base::TimeTicks ignored;
    const int frame_number = oracle.next_frame_number();
    oracle.RecordCapture(2.0);
    ASSERT_TRUE(oracle.CompleteCapture(frame_number, true, &ignored));
  }
}

}  // namespace media
