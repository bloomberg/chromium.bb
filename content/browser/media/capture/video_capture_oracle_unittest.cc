// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/video_capture_oracle.h"

#include <cstdlib>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
namespace {

bool AddEventAndConsiderSampling(SmoothEventSampler* sampler,
                                 base::TimeTicks event_time) {
  sampler->ConsiderPresentationEvent(event_time);
  return sampler->ShouldSample();
}

void SteadyStateSampleAndAdvance(base::TimeDelta vsync,
                                 SmoothEventSampler* sampler,
                                 base::TimeTicks* t) {
  ASSERT_TRUE(AddEventAndConsiderSampling(sampler, *t));
  ASSERT_TRUE(sampler->HasUnrecordedEvent());
  sampler->RecordSample();
  ASSERT_FALSE(sampler->HasUnrecordedEvent());
  ASSERT_FALSE(sampler->IsOverdueForSamplingAt(*t));
  *t += vsync;
  ASSERT_FALSE(sampler->IsOverdueForSamplingAt(*t));
}

void SteadyStateNoSampleAndAdvance(base::TimeDelta vsync,
                                   SmoothEventSampler* sampler,
                                   base::TimeTicks* t) {
  ASSERT_FALSE(AddEventAndConsiderSampling(sampler, *t));
  ASSERT_TRUE(sampler->HasUnrecordedEvent());
  ASSERT_FALSE(sampler->IsOverdueForSamplingAt(*t));
  *t += vsync;
  ASSERT_FALSE(sampler->IsOverdueForSamplingAt(*t));
}

base::TimeTicks InitialTestTimeTicks() {
  return base::TimeTicks() + base::TimeDelta::FromSeconds(1);
}

void TestRedundantCaptureStrategy(base::TimeDelta capture_period,
                                  int redundant_capture_goal,
                                  SmoothEventSampler* sampler,
                                  base::TimeTicks* t) {
  // Before any events have been considered, we're overdue for sampling.
  ASSERT_TRUE(sampler->IsOverdueForSamplingAt(*t));

  // Consider the first event.  We want to sample that.
  ASSERT_FALSE(sampler->HasUnrecordedEvent());
  ASSERT_TRUE(AddEventAndConsiderSampling(sampler, *t));
  ASSERT_TRUE(sampler->HasUnrecordedEvent());
  sampler->RecordSample();
  ASSERT_FALSE(sampler->HasUnrecordedEvent());

  // After more than 250 ms has passed without considering an event, we should
  // repeatedly be overdue for sampling.  However, once the redundant capture
  // goal is achieved, we should no longer be overdue for sampling.
  *t += base::TimeDelta::FromMilliseconds(250);
  for (int i = 0; i < redundant_capture_goal; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_FALSE(sampler->HasUnrecordedEvent());
    ASSERT_TRUE(sampler->IsOverdueForSamplingAt(*t))
        << "Should sample until redundant capture goal is hit";
    sampler->RecordSample();
    *t += capture_period;  // Timer fires once every capture period.
  }
  ASSERT_FALSE(sampler->IsOverdueForSamplingAt(*t))
      << "Should not be overdue once redundant capture goal achieved.";
}

}  // namespace

// 60Hz sampled at 30Hz should produce 30Hz.  In addition, this test contains
// much more comprehensive before/after/edge-case scenarios than the others.
TEST(SmoothEventSamplerTest, Sample60HertzAt30Hertz) {
  const base::TimeDelta capture_period = base::TimeDelta::FromSeconds(1) / 30;
  const int redundant_capture_goal = 200;
  const base::TimeDelta vsync = base::TimeDelta::FromSeconds(1) / 60;

  SmoothEventSampler sampler(capture_period, true, redundant_capture_goal);
  base::TimeTicks t = InitialTestTimeTicks();

  TestRedundantCaptureStrategy(capture_period, redundant_capture_goal,
                               &sampler, &t);

  // Steady state, we should capture every other vsync, indefinitely.
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 20; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_EQ(i >= 14, sampler.IsOverdueForSamplingAt(t));
    ASSERT_TRUE(AddEventAndConsiderSampling(&sampler, t));
    ASSERT_TRUE(sampler.HasUnrecordedEvent());
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state,
  // but at a different phase.
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t));
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }
}

// 50Hz sampled at 30Hz should produce a sequence where some frames are skipped.
TEST(SmoothEventSamplerTest, Sample50HertzAt30Hertz) {
  const base::TimeDelta capture_period = base::TimeDelta::FromSeconds(1) / 30;
  const int redundant_capture_goal = 2;
  const base::TimeDelta vsync = base::TimeDelta::FromSeconds(1) / 50;

  SmoothEventSampler sampler(capture_period, true, redundant_capture_goal);
  base::TimeTicks t = InitialTestTimeTicks();

  TestRedundantCaptureStrategy(capture_period, redundant_capture_goal,
                               &sampler, &t);

  // Steady state, we should capture 1st, 2nd and 4th frames out of every five
  // frames, indefinitely.
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 20; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_EQ(i >= 11, sampler.IsOverdueForSamplingAt(t));
    ASSERT_TRUE(AddEventAndConsiderSampling(&sampler, t));
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state
  // again.
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t));
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }
}

// 75Hz sampled at 30Hz should produce a sequence where some frames are skipped.
TEST(SmoothEventSamplerTest, Sample75HertzAt30Hertz) {
  const base::TimeDelta capture_period = base::TimeDelta::FromSeconds(1) / 30;
  const int redundant_capture_goal = 32;
  const base::TimeDelta vsync = base::TimeDelta::FromSeconds(1) / 75;

  SmoothEventSampler sampler(capture_period, true, redundant_capture_goal);
  base::TimeTicks t = InitialTestTimeTicks();

  TestRedundantCaptureStrategy(capture_period, redundant_capture_goal,
                               &sampler, &t);

  // Steady state, we should capture 1st and 3rd frames out of every five
  // frames, indefinitely.
  SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 20; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_EQ(i >= 16, sampler.IsOverdueForSamplingAt(t));
    ASSERT_TRUE(AddEventAndConsiderSampling(&sampler, t));
    t += vsync;
  }

  // Now suppose we can sample again. We capture the next frame, and not the one
  // after that, and then we're back in the steady state again.
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t));
  SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }
}

// 30Hz sampled at 30Hz should produce 30Hz.
TEST(SmoothEventSamplerTest, Sample30HertzAt30Hertz) {
  const base::TimeDelta capture_period = base::TimeDelta::FromSeconds(1) / 30;
  const int redundant_capture_goal = 1;
  const base::TimeDelta vsync = base::TimeDelta::FromSeconds(1) / 30;

  SmoothEventSampler sampler(capture_period, true, redundant_capture_goal);
  base::TimeTicks t = InitialTestTimeTicks();

  TestRedundantCaptureStrategy(capture_period, redundant_capture_goal,
                               &sampler, &t);

  // Steady state, we should capture every vsync, indefinitely.
  for (int i = 0; i < 200; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 10; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_EQ(i >= 7, sampler.IsOverdueForSamplingAt(t));
    ASSERT_TRUE(AddEventAndConsiderSampling(&sampler, t));
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state.
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t));
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }
}

// 24Hz sampled at 30Hz should produce 24Hz.
TEST(SmoothEventSamplerTest, Sample24HertzAt30Hertz) {
  const base::TimeDelta capture_period = base::TimeDelta::FromSeconds(1) / 30;
  const int redundant_capture_goal = 333;
  const base::TimeDelta vsync = base::TimeDelta::FromSeconds(1) / 24;

  SmoothEventSampler sampler(capture_period, true, redundant_capture_goal);
  base::TimeTicks t = InitialTestTimeTicks();

  TestRedundantCaptureStrategy(capture_period, redundant_capture_goal,
                               &sampler, &t);

  // Steady state, we should capture every vsync, indefinitely.
  for (int i = 0; i < 200; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 10; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_EQ(i >= 6, sampler.IsOverdueForSamplingAt(t));
    ASSERT_TRUE(AddEventAndConsiderSampling(&sampler, t));
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state.
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t));
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }
}

TEST(SmoothEventSamplerTest, DoubleDrawAtOneTimeStillDirties) {
  const base::TimeDelta capture_period = base::TimeDelta::FromSeconds(1) / 30;
  const base::TimeDelta overdue_period = base::TimeDelta::FromSeconds(1);

  SmoothEventSampler sampler(capture_period, true, 1);
  base::TimeTicks t = InitialTestTimeTicks();

  ASSERT_TRUE(AddEventAndConsiderSampling(&sampler, t));
  sampler.RecordSample();
  ASSERT_FALSE(sampler.IsOverdueForSamplingAt(t))
      << "Sampled last event; should not be dirty.";
  t += overdue_period;

  // Now simulate 2 events with the same clock value.
  ASSERT_TRUE(AddEventAndConsiderSampling(&sampler, t));
  sampler.RecordSample();
  ASSERT_FALSE(AddEventAndConsiderSampling(&sampler, t))
      << "Two events at same time -- expected second not to be sampled.";
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t + overdue_period))
      << "Second event should dirty the capture state.";
  sampler.RecordSample();
  ASSERT_FALSE(sampler.IsOverdueForSamplingAt(t + overdue_period));
}

TEST(SmoothEventSamplerTest, FallbackToPollingIfUpdatesUnreliable) {
  const base::TimeDelta timer_interval = base::TimeDelta::FromSeconds(1) / 30;

  SmoothEventSampler should_not_poll(timer_interval, true, 1);
  SmoothEventSampler should_poll(timer_interval, false, 1);
  base::TimeTicks t = InitialTestTimeTicks();

  // Do one round of the "happy case" where an event was received and
  // RecordSample() was called by the client.
  ASSERT_TRUE(AddEventAndConsiderSampling(&should_not_poll, t));
  ASSERT_TRUE(AddEventAndConsiderSampling(&should_poll, t));
  should_not_poll.RecordSample();
  should_poll.RecordSample();

  // For the following time period, before 250 ms has elapsed, neither sampler
  // says we're overdue.
  const int non_overdue_intervals = static_cast<int>(
      base::TimeDelta::FromMilliseconds(250) / timer_interval);
  for (int i = 0; i < non_overdue_intervals; i++) {
    t += timer_interval;
    ASSERT_FALSE(should_not_poll.IsOverdueForSamplingAt(t))
        << "Sampled last event; should not be dirty.";
    ASSERT_FALSE(should_poll.IsOverdueForSamplingAt(t))
        << "Dirty interval has not elapsed yet.";
  }

  // Next time period ahead, both samplers say we're overdue.  The non-polling
  // sampler is returning true here because it has been configured to allow one
  // redundant capture.
  t += timer_interval;  // Step past the 250 ms threshold.
  ASSERT_TRUE(should_not_poll.IsOverdueForSamplingAt(t))
      << "Sampled last event; is dirty one time only to meet redundancy goal.";
  ASSERT_TRUE(should_poll.IsOverdueForSamplingAt(t))
      << "If updates are unreliable, must fall back to polling when idle.";
  should_not_poll.RecordSample();
  should_poll.RecordSample();

  // Forever more, the non-polling sampler returns false while the polling one
  // returns true.
  for (int i = 0; i < 100; ++i) {
    t += timer_interval;
    ASSERT_FALSE(should_not_poll.IsOverdueForSamplingAt(t))
        << "Sampled last event; should not be dirty.";
    ASSERT_TRUE(should_poll.IsOverdueForSamplingAt(t))
        << "If updates are unreliable, must fall back to polling when idle.";
    should_poll.RecordSample();
  }
  t += timer_interval / 3;
  ASSERT_FALSE(should_not_poll.IsOverdueForSamplingAt(t))
      << "Sampled last event; should not be dirty.";
  ASSERT_TRUE(should_poll.IsOverdueForSamplingAt(t))
      << "If updates are unreliable, must fall back to polling when idle.";
  should_poll.RecordSample();
}

namespace {

struct DataPoint {
  bool should_capture;
  double increment_ms;
};

void ReplayCheckingSamplerDecisions(const DataPoint* data_points,
                                    size_t num_data_points,
                                    SmoothEventSampler* sampler) {
  base::TimeTicks t = InitialTestTimeTicks();
  for (size_t i = 0; i < num_data_points; ++i) {
    t += base::TimeDelta::FromMicroseconds(
        static_cast<int64>(data_points[i].increment_ms * 1000));
    ASSERT_EQ(data_points[i].should_capture,
              AddEventAndConsiderSampling(sampler, t))
        << "at data_points[" << i << ']';
    if (data_points[i].should_capture)
      sampler->RecordSample();
  }
}

}  // namespace

TEST(SmoothEventSamplerTest, DrawingAt24FpsWith60HzVsyncSampledAt30Hertz) {
  // Actual capturing of timing data: Initial instability as a 24 FPS video was
  // started from a still screen, then clearly followed by steady-state.
  static const DataPoint data_points[] = {
    { true, 1437.93 }, { true, 150.484 }, { true, 217.362 }, { true, 50.161 },
    { true, 33.44 }, { false, 0 }, { true, 16.721 }, { true, 66.88 },
    { true, 50.161 }, { false, 0 }, { false, 0 }, { true, 50.16 },
    { true, 33.441 }, { true, 16.72 }, { false, 16.72 }, { true, 117.041 },
    { true, 16.72 }, { false, 16.72 }, { true, 50.161 }, { true, 50.16 },
    { true, 33.441 }, { true, 33.44 }, { true, 33.44 }, { true, 16.72 },
    { false, 0 }, { true, 50.161 }, { false, 0 }, { true, 33.44 },
    { true, 16.72 }, { false, 16.721 }, { true, 66.881 }, { false, 0 },
    { true, 33.441 }, { true, 16.72 }, { true, 50.16 }, { true, 16.72 },
    { false, 16.721 }, { true, 50.161 }, { true, 50.16 }, { false, 0 },
    { true, 33.441 }, { true, 50.337 }, { true, 50.183 }, { true, 16.722 },
    { true, 50.161 }, { true, 33.441 }, { true, 50.16 }, { true, 33.441 },
    { true, 50.16 }, { true, 33.441 }, { true, 50.16 }, { true, 33.44 },
    { true, 50.161 }, { true, 50.16 }, { true, 33.44 }, { true, 33.441 },
    { true, 50.16 }, { true, 50.161 }, { true, 33.44 }, { true, 33.441 },
    { true, 50.16 }, { true, 33.44 }, { true, 50.161 }, { true, 33.44 },
    { true, 50.161 }, { true, 33.44 }, { true, 50.161 }, { true, 33.44 },
    { true, 83.601 }, { true, 16.72 }, { true, 33.44 }, { false, 0 }
  };

  SmoothEventSampler sampler(base::TimeDelta::FromSeconds(1) / 30, true, 3);
  ReplayCheckingSamplerDecisions(data_points, arraysize(data_points), &sampler);
}

TEST(SmoothEventSamplerTest, DrawingAt30FpsWith60HzVsyncSampledAt30Hertz) {
  // Actual capturing of timing data: Initial instability as a 30 FPS video was
  // started from a still screen, then followed by steady-state.  Drawing
  // framerate from the video rendering was a bit volatile, but averaged 30 FPS.
  static const DataPoint data_points[] = {
    { true, 2407.69 }, { true, 16.733 }, { true, 217.362 }, { true, 33.441 },
    { true, 33.44 }, { true, 33.44 }, { true, 33.441 }, { true, 33.44 },
    { true, 33.44 }, { true, 33.441 }, { true, 33.44 }, { true, 33.44 },
    { true, 16.721 }, { true, 33.44 }, { false, 0 }, { true, 50.161 },
    { true, 50.16 }, { false, 0 }, { true, 50.161 }, { true, 33.44 },
    { true, 16.72 }, { false, 0 }, { false, 16.72 }, { true, 66.881 },
    { false, 0 }, { true, 33.44 }, { true, 16.72 }, { true, 50.161 },
    { false, 0 }, { true, 33.538 }, { true, 33.526 }, { true, 33.447 },
    { true, 33.445 }, { true, 33.441 }, { true, 16.721 }, { true, 33.44 },
    { true, 33.44 }, { true, 50.161 }, { true, 16.72 }, { true, 33.44 },
    { true, 33.441 }, { true, 33.44 }, { false, 0 }, { false, 16.72 },
    { true, 66.881 }, { true, 16.72 }, { false, 16.72 }, { true, 50.16 },
    { true, 33.441 }, { true, 33.44 }, { true, 33.44 }, { true, 33.44 },
    { true, 33.441 }, { true, 33.44 }, { true, 50.161 }, { false, 0 },
    { true, 33.44 }, { true, 33.44 }, { true, 50.161 }, { true, 16.72 },
    { true, 33.44 }, { true, 33.441 }, { false, 0 }, { true, 66.88 },
    { true, 33.441 }, { true, 33.44 }, { true, 33.44 }, { false, 0 },
    { true, 33.441 }, { true, 33.44 }, { true, 33.44 }, { false, 0 },
    { true, 16.72 }, { true, 50.161 }, { false, 0 }, { true, 50.16 },
    { false, 0.001 }, { true, 16.721 }, { true, 66.88 }, { true, 33.44 },
    { true, 33.441 }, { true, 33.44 }, { true, 50.161 }, { true, 16.72 },
    { false, 0 }, { true, 33.44 }, { false, 16.72 }, { true, 66.881 },
    { true, 33.44 }, { true, 16.72 }, { true, 33.441 }, { false, 16.72 },
    { true, 66.88 }, { true, 16.721 }, { true, 50.16 }, { true, 33.44 },
    { true, 16.72 }, { true, 33.441 }, { true, 33.44 }, { true, 33.44 }
  };

  SmoothEventSampler sampler(base::TimeDelta::FromSeconds(1) / 30, true, 3);
  ReplayCheckingSamplerDecisions(data_points, arraysize(data_points), &sampler);
}

TEST(SmoothEventSamplerTest, DrawingAt60FpsWith60HzVsyncSampledAt30Hertz) {
  // Actual capturing of timing data: WebGL Acquarium demo
  // (http://webglsamples.googlecode.com/hg/aquarium/aquarium.html) which ran
  // between 55-60 FPS in the steady-state.
  static const DataPoint data_points[] = {
    { true, 16.72 }, { true, 16.72 }, { true, 4163.29 }, { true, 50.193 },
    { true, 117.041 }, { true, 50.161 }, { true, 50.16 }, { true, 33.441 },
    { true, 50.16 }, { true, 33.44 }, { false, 0 }, { false, 0 },
    { true, 50.161 }, { true, 83.601 }, { true, 50.16 }, { true, 16.72 },
    { true, 33.441 }, { false, 16.72 }, { true, 50.16 }, { true, 16.72 },
    { false, 0.001 }, { true, 33.441 }, { false, 16.72 }, { true, 16.72 },
    { true, 50.16 }, { false, 0 }, { true, 16.72 }, { true, 33.441 },
    { false, 0 }, { true, 33.44 }, { false, 16.72 }, { true, 16.72 },
    { true, 50.161 }, { false, 0 }, { true, 16.72 }, { true, 33.44 },
    { false, 0 }, { true, 33.44 }, { false, 16.721 }, { true, 16.721 },
    { true, 50.161 }, { false, 0 }, { true, 16.72 }, { true, 33.441 },
    { false, 0 }, { true, 33.44 }, { false, 16.72 }, { true, 33.44 },
    { false, 0 }, { true, 16.721 }, { true, 50.161 }, { false, 0 },
    { true, 33.44 }, { false, 0 }, { true, 16.72 }, { true, 33.441 },
    { false, 0 }, { true, 33.44 }, { false, 16.72 }, { true, 16.72 },
    { true, 50.16 }, { false, 0 }, { true, 16.721 }, { true, 33.44 },
    { false, 0 }, { true, 33.44 }, { false, 16.721 }, { true, 16.721 },
    { true, 50.161 }, { false, 0 }, { true, 16.72 }, { true, 33.44 },
    { false, 0 }, { true, 33.441 }, { false, 16.72 }, { true, 16.72 },
    { true, 50.16 }, { false, 0 }, { true, 16.72 }, { true, 33.441 },
    { true, 33.44 }, { false, 0 }, { true, 33.44 }, { true, 33.441 },
    { false, 0 }, { true, 33.44 }, { true, 33.441 }, { false, 0 },
    { true, 33.44 }, { false, 0 }, { true, 33.44 }, { false, 16.72 },
    { true, 16.721 }, { true, 50.161 }, { false, 0 }, { true, 16.72 },
    { true, 33.44 }, { true, 33.441 }, { false, 0 }, { true, 33.44 },
    { true, 33.44 }, { false, 0 }, { true, 33.441 }, { false, 16.72 },
    { true, 16.72 }, { true, 50.16 }, { false, 0 }, { true, 16.72 },
    { true, 33.441 }, { false, 0 }, { true, 33.44 }, { false, 16.72 },
    { true, 33.44 }, { false, 0 }, { true, 16.721 }, { true, 50.161 },
    { false, 0 }, { true, 16.72 }, { true, 33.44 }, { false, 0 },
    { true, 33.441 }, { false, 16.72 }, { true, 16.72 }, { true, 50.16 }
  };

  SmoothEventSampler sampler(base::TimeDelta::FromSeconds(1) / 30, true, 3);
  ReplayCheckingSamplerDecisions(data_points, arraysize(data_points), &sampler);
}

class AnimatedContentSamplerTest : public ::testing::Test {
 public:
  AnimatedContentSamplerTest() {}
  virtual ~AnimatedContentSamplerTest() {}

  virtual void SetUp() OVERRIDE {
    const base::TimeDelta since_epoch =
        InitialTestTimeTicks() - base::TimeTicks::UnixEpoch();
    rand_seed_ = abs(static_cast<int>(since_epoch.InMicroseconds()));
    sampler_.reset(new AnimatedContentSampler(GetMinCapturePeriod()));
  }

 protected:
  // Overridden by subclass for parameterized tests.
  virtual base::TimeDelta GetMinCapturePeriod() const {
    return base::TimeDelta::FromSeconds(1) / 30;
  }

  AnimatedContentSampler* sampler() const {
    return sampler_.get();
  }

  int GetRandomInRange(int begin, int end) {
    const int len = end - begin;
    const int rand_offset = (len == 0) ? 0 : (NextRandomInt() % (end - begin));
    return begin + rand_offset;
  }

  gfx::Rect GetRandomDamageRect() {
    return gfx::Rect(0, 0, GetRandomInRange(1, 100), GetRandomInRange(1, 100));
  }

  gfx::Rect GetContentDamageRect() {
    // This must be distinct from anything GetRandomDamageRect() could return.
    return gfx::Rect(0, 0, 1280, 720);
  }

  // Directly inject an observation.  Only used to test
  // ElectMajorityDamageRect().
  void ObserveDamageRect(const gfx::Rect& damage_rect) {
    sampler_->observations_.push_back(
        AnimatedContentSampler::Observation(damage_rect, base::TimeTicks()));
  }

  gfx::Rect ElectMajorityDamageRect() const {
    return sampler_->ElectMajorityDamageRect();
  }

 private:
  // Note: Not using base::RandInt() because it is horribly slow on debug
  // builds.  The following is a very simple, deterministic LCG:
  int NextRandomInt() {
    rand_seed_ = (1103515245 * rand_seed_ + 12345) % (1 << 31);
    return rand_seed_;
  }

  int rand_seed_;
  scoped_ptr<AnimatedContentSampler> sampler_;
};

TEST_F(AnimatedContentSamplerTest, ElectsNoneFromZeroDamageRects) {
  EXPECT_EQ(gfx::Rect(), ElectMajorityDamageRect());
}

TEST_F(AnimatedContentSamplerTest, ElectsMajorityFromOneDamageRect) {
  const gfx::Rect the_one_rect(0, 0, 1, 1);
  ObserveDamageRect(the_one_rect);
  EXPECT_EQ(the_one_rect, ElectMajorityDamageRect());
}

TEST_F(AnimatedContentSamplerTest, ElectsNoneFromTwoDamageRectsOfSameArea) {
  const gfx::Rect one_rect(0, 0, 1, 1);
  const gfx::Rect another_rect(1, 1, 1, 1);
  ObserveDamageRect(one_rect);
  ObserveDamageRect(another_rect);
  EXPECT_EQ(gfx::Rect(), ElectMajorityDamageRect());
}

TEST_F(AnimatedContentSamplerTest, ElectsLargerOfTwoDamageRects_1) {
  const gfx::Rect one_rect(0, 0, 1, 1);
  const gfx::Rect another_rect(0, 0, 2, 2);
  ObserveDamageRect(one_rect);
  ObserveDamageRect(another_rect);
  EXPECT_EQ(another_rect, ElectMajorityDamageRect());
}

TEST_F(AnimatedContentSamplerTest, ElectsLargerOfTwoDamageRects_2) {
  const gfx::Rect one_rect(0, 0, 2, 2);
  const gfx::Rect another_rect(0, 0, 1, 1);
  ObserveDamageRect(one_rect);
  ObserveDamageRect(another_rect);
  EXPECT_EQ(one_rect, ElectMajorityDamageRect());
}

TEST_F(AnimatedContentSamplerTest, ElectsSameAsMooreDemonstration) {
  // A more complex sequence (from Moore's web site): Three different Rects with
  // the same area, but occurring a different number of times.  C should win the
  // vote.
  const gfx::Rect rect_a(0, 0, 1, 4);
  const gfx::Rect rect_b(1, 1, 4, 1);
  const gfx::Rect rect_c(2, 2, 2, 2);
  for (int i = 0; i < 3; ++i)
    ObserveDamageRect(rect_a);
  for (int i = 0; i < 2; ++i)
    ObserveDamageRect(rect_c);
  for (int i = 0; i < 2; ++i)
    ObserveDamageRect(rect_b);
  for (int i = 0; i < 3; ++i)
    ObserveDamageRect(rect_c);
  ObserveDamageRect(rect_b);
  for (int i = 0; i < 2; ++i)
    ObserveDamageRect(rect_c);
  EXPECT_EQ(rect_c, ElectMajorityDamageRect());
}

TEST_F(AnimatedContentSamplerTest, Elects24FpsVideoInsteadOf48FpsSpinner) {
  // Scenario: 24 FPS 720x480 Video versus 48 FPS 96x96 "Busy Spinner"
  const gfx::Rect video_rect(100, 100, 720, 480);
  const gfx::Rect spinner_rect(360, 0, 96, 96);
  for (int i = 0; i < 100; ++i) {
    // |video_rect| occurs once for every two |spinner_rect|.  Vary the order
    // of events between the two:
    ObserveDamageRect(video_rect);
    ObserveDamageRect(spinner_rect);
    ObserveDamageRect(spinner_rect);
    ObserveDamageRect(video_rect);
    ObserveDamageRect(spinner_rect);
    ObserveDamageRect(spinner_rect);
    ObserveDamageRect(spinner_rect);
    ObserveDamageRect(video_rect);
    ObserveDamageRect(spinner_rect);
    ObserveDamageRect(spinner_rect);
    ObserveDamageRect(video_rect);
    ObserveDamageRect(spinner_rect);
  }
  EXPECT_EQ(video_rect, ElectMajorityDamageRect());
}

namespace {

// A test scenario for AnimatedContentSamplerParameterizedTest.
struct Scenario {
  base::TimeDelta vsync_interval;  // Reflects compositor's update rate.
  base::TimeDelta min_capture_period;  // Reflects maximum capture rate.
  base::TimeDelta content_period;  // Reflects content animation rate.

  Scenario(base::TimeDelta v, base::TimeDelta m, base::TimeDelta c)
      : vsync_interval(v), min_capture_period(m), content_period(c) {
    CHECK(content_period >= vsync_interval)
        << "Bad test params: Impossible to animate faster than the compositor.";
  }
};

// Value printer for Scenario.
::std::ostream& operator<<(::std::ostream& os, const Scenario& s) {
  return os << "{ vsync_interval=" << s.vsync_interval.InMicroseconds()
            << ", min_capture_period=" << s.min_capture_period.InMicroseconds()
            << ", content_period=" << s.content_period.InMicroseconds()
            << " }";
}

base::TimeDelta FpsAsPeriod(int frame_rate) {
  return base::TimeDelta::FromSeconds(1) / frame_rate;
}

}  // namespace

class AnimatedContentSamplerParameterizedTest
    : public AnimatedContentSamplerTest,
      public ::testing::WithParamInterface<Scenario> {
 public:
  AnimatedContentSamplerParameterizedTest()
      : count_dropped_frames_(0), count_sampled_frames_(0) {}
  virtual ~AnimatedContentSamplerParameterizedTest() {}

 protected:
  typedef std::pair<gfx::Rect, base::TimeTicks> Event;

  virtual base::TimeDelta GetMinCapturePeriod() const OVERRIDE {
    return GetParam().min_capture_period;
  }

  // Generate a sequence of events from the compositor pipeline.  The event
  // times will all be at compositor vsync boundaries.
  std::vector<Event> GenerateEventSequence(base::TimeTicks begin,
                                           base::TimeTicks end,
                                           bool include_content_frame_events,
                                           bool include_random_events) {
    DCHECK(GetParam().content_period >= GetParam().vsync_interval);
    base::TimeTicks next_content_time = begin - GetParam().content_period;
    std::vector<Event> events;
    for (base::TimeTicks compositor_time = begin; compositor_time < end;
         compositor_time += GetParam().vsync_interval) {
      if (include_content_frame_events && next_content_time < compositor_time) {
        events.push_back(Event(GetContentDamageRect(), compositor_time));
        next_content_time += GetParam().content_period;
      } else if (include_random_events && GetRandomInRange(0, 1) == 0) {
        events.push_back(Event(GetRandomDamageRect(), compositor_time));
      }
    }

    DCHECK(!events.empty());
    return events;
  }

  // Feed |events| through the sampler, and detect whether the expected
  // lock-in/out transition occurs.  Also, track and measure the frame drop
  // ratio and check it against the expected drop rate.
  void RunEventSequence(const std::vector<Event> events,
                        bool was_detecting_before,
                        bool is_detecting_after,
                        bool simulate_pipeline_back_pressure) {
    gfx::Rect first_detected_region;

    EXPECT_EQ(was_detecting_before, sampler()->HasProposal());
    bool has_detection_switched = false;
    ResetFrameCounters();
    for (std::vector<Event>::const_iterator i = events.begin();
         i != events.end(); ++i) {
      sampler()->ConsiderPresentationEvent(i->first, i->second);

      // Detect when the sampler locks in/out, and that it stays that way for
      // all further iterations of this loop.
      if (!has_detection_switched &&
          was_detecting_before != sampler()->HasProposal()) {
        has_detection_switched = true;
      }
      ASSERT_EQ(
          has_detection_switched ? is_detecting_after : was_detecting_before,
          sampler()->HasProposal());

      if (sampler()->HasProposal()) {
        // Make sure the sampler doesn't flip-flop and keep proposing sampling
        // based on locking into different regions.
        if (first_detected_region.IsEmpty()) {
          first_detected_region = sampler()->detected_region();
          ASSERT_FALSE(first_detected_region.IsEmpty());
        } else {
          EXPECT_EQ(first_detected_region, sampler()->detected_region());
        }

        if (simulate_pipeline_back_pressure && GetRandomInRange(0, 2) == 0)
          ClientCannotSampleFrame(*i);
        else
          ClientDoesWhatSamplerProposes(*i);
      } else {
        EXPECT_FALSE(sampler()->ShouldSample());
        if (!simulate_pipeline_back_pressure || GetRandomInRange(0, 2) == 1)
          sampler()->RecordSample(i->second);
      }
    }
    EXPECT_EQ(is_detecting_after, sampler()->HasProposal());
    ExpectFrameDropRatioIsCorrect();
  }

  void ResetFrameCounters() {
    count_dropped_frames_ = 0;
    count_sampled_frames_ = 0;
  }

  // Keep track what the sampler is proposing, and call RecordSample() if it
  // proposes sampling |event|.
  void ClientDoesWhatSamplerProposes(const Event& event) {
    if (sampler()->ShouldSample()) {
      EXPECT_EQ(GetContentDamageRect(), event.first);
      sampler()->RecordSample(sampler()->frame_timestamp());
      ++count_sampled_frames_;
    } else if (event.first == GetContentDamageRect()) {
      ++count_dropped_frames_;
    }
  }

  // RecordSample() is not called, but for testing, keep track of what the
  // sampler is proposing for |event|.
  void ClientCannotSampleFrame(const Event& event) {
    if (sampler()->ShouldSample()) {
      EXPECT_EQ(GetContentDamageRect(), event.first);
      ++count_sampled_frames_;
    } else if (event.first == GetContentDamageRect()) {
      ++count_dropped_frames_;
    }
  }

  // Confirm the AnimatedContentSampler is not dropping more frames than
  // expected, given current test parameters.
  void ExpectFrameDropRatioIsCorrect() {
    if (count_sampled_frames_ == 0) {
      EXPECT_EQ(0, count_dropped_frames_);
      return;
    }
    const double content_framerate =
        1000000.0 / GetParam().content_period.InMicroseconds();
    const double capture_framerate =
        1000000.0 / GetParam().min_capture_period.InMicroseconds();
    const double expected_drop_rate = std::max(
        0.0, (content_framerate - capture_framerate) / capture_framerate);
    const double actual_drop_rate =
        static_cast<double>(count_dropped_frames_) / count_sampled_frames_;
    EXPECT_NEAR(expected_drop_rate, actual_drop_rate, 0.015);
  }

 private:
  // These counters only include the frames with the desired content.
  int count_dropped_frames_;
  int count_sampled_frames_;
};

// Tests that the implementation locks in/out of frames containing stable
// animated content, whether or not random events are also simultaneously
// present.
TEST_P(AnimatedContentSamplerParameterizedTest, DetectsAnimatedContent) {
  // |begin| refers to the start of an event sequence in terms of the
  // Compositor's clock.
  base::TimeTicks begin = InitialTestTimeTicks();

  // Provide random events and expect no lock-in.
  base::TimeTicks end = begin + base::TimeDelta::FromSeconds(5);
  RunEventSequence(GenerateEventSequence(begin, end, false, true),
                   false,
                   false,
                   false);
  begin = end;

  // Provide content frame events with some random events mixed-in, and expect
  // the sampler to lock-in.
  end = begin + base::TimeDelta::FromSeconds(5);
  RunEventSequence(GenerateEventSequence(begin, end, true, true),
                   false,
                   true,
                   false);
  begin = end;

  // Continue providing content frame events without the random events mixed-in
  // and expect the lock-in to hold.
  end = begin + base::TimeDelta::FromSeconds(5);
  RunEventSequence(GenerateEventSequence(begin, end, true, false),
                   true,
                   true,
                   false);
  begin = end;

  // Continue providing just content frame events and expect the lock-in to
  // hold.  Also simulate the capture pipeline experiencing back pressure.
  end = begin + base::TimeDelta::FromSeconds(20);
  RunEventSequence(GenerateEventSequence(begin, end, true, false),
                   true,
                   true,
                   true);
  begin = end;

  // Provide a half-second of random events only, and expect the lock-in to be
  // broken.
  end = begin + base::TimeDelta::FromMilliseconds(500);
  RunEventSequence(GenerateEventSequence(begin, end, false, true),
                   true,
                   false,
                   false);
  begin = end;

  // Now, go back to providing content frame events, and expect the sampler to
  // lock-in once again.
  end = begin + base::TimeDelta::FromSeconds(5);
  RunEventSequence(GenerateEventSequence(begin, end, true, false),
                   false,
                   true,
                   false);
  begin = end;
}

// Tests that AnimatedContentSampler won't lock in to, nor flip-flop between,
// two animations of the same pixel change rate.  VideoCaptureOracle should
// revert to using the SmoothEventSampler for these kinds of situations, as
// there is no "right answer" as to which animation to lock into.
TEST_P(AnimatedContentSamplerParameterizedTest,
       DoesNotLockInToTwoCompetingAnimations) {
  // Don't test when the event stream cannot indicate two separate content
  // animations under the current test parameters.
  if (GetParam().content_period < 2 * GetParam().vsync_interval)
    return;

  // Start the first animation and run for a bit, and expect the sampler to
  // lock-in.
  base::TimeTicks begin = InitialTestTimeTicks();
  base::TimeTicks end = begin + base::TimeDelta::FromSeconds(5);
  RunEventSequence(GenerateEventSequence(begin, end, true, false),
                   false,
                   true,
                   false);
  begin = end;

  // Now, keep the first animation and blend in an second animation of the same
  // size and frame rate, but at a different position.  This will should cause
  // the sampler to enter an "undetected" state since it's unclear which
  // animation should be locked into.
  end = begin + base::TimeDelta::FromSeconds(20);
  std::vector<Event> first_animation_events =
      GenerateEventSequence(begin, end, true, false);
  gfx::Rect second_animation_rect(
      gfx::Point(0, GetContentDamageRect().height()),
      GetContentDamageRect().size());
  std::vector<Event> both_animations_events;
  base::TimeDelta second_animation_offset = GetParam().vsync_interval;
  for (std::vector<Event>::const_iterator i = first_animation_events.begin();
       i != first_animation_events.end(); ++i) {
    both_animations_events.push_back(*i);
    both_animations_events.push_back(
        Event(second_animation_rect, i->second + second_animation_offset));
  }
  RunEventSequence(both_animations_events, true, false, false);
  begin = end;

  // Now, run just the first animation, and expect the sampler to lock-in once
  // again.
  end = begin + base::TimeDelta::FromSeconds(5);
  RunEventSequence(GenerateEventSequence(begin, end, true, false),
                   false,
                   true,
                   false);
  begin = end;

  // Now, blend in the second animation again, but it has half the frame rate of
  // the first animation and damage Rects with twice the area.  This will should
  // cause the sampler to enter an "undetected" state again.  This tests that
  // pixel-weighting is being accounted for in the sampler's logic.
  end = begin + base::TimeDelta::FromSeconds(20);
  first_animation_events = GenerateEventSequence(begin, end, true, false);
  second_animation_rect.set_width(second_animation_rect.width() * 2);
  both_animations_events.clear();
  bool include_second_animation_frame = true;
  for (std::vector<Event>::const_iterator i = first_animation_events.begin();
       i != first_animation_events.end(); ++i) {
    both_animations_events.push_back(*i);
    if (include_second_animation_frame) {
      both_animations_events.push_back(
          Event(second_animation_rect, i->second + second_animation_offset));
    }
    include_second_animation_frame = !include_second_animation_frame;
  }
  RunEventSequence(both_animations_events, true, false, false);
  begin = end;
}

// Tests that the frame timestamps are smooth; meaning, that when run through a
// simulated compositor, each frame is held displayed for the right number of
// v-sync intervals.
TEST_P(AnimatedContentSamplerParameterizedTest, FrameTimestampsAreSmooth) {
  // Generate 30 seconds of animated content events, run the events through
  // AnimatedContentSampler, and record all frame timestamps being proposed
  // once lock-in is continuous.
  base::TimeTicks begin = InitialTestTimeTicks();
  std::vector<Event> events = GenerateEventSequence(
      begin,
      begin + base::TimeDelta::FromSeconds(20),
      true,
      false);
  typedef std::vector<base::TimeTicks> Timestamps;
  Timestamps frame_timestamps;
  for (std::vector<Event>::const_iterator i = events.begin(); i != events.end();
       ++i) {
    sampler()->ConsiderPresentationEvent(i->first, i->second);
    if (sampler()->HasProposal()) {
      if (sampler()->ShouldSample()) {
        frame_timestamps.push_back(sampler()->frame_timestamp());
        sampler()->RecordSample(sampler()->frame_timestamp());
      }
    } else {
      frame_timestamps.clear();  // Reset until continuous lock-in.
    }
  }
  ASSERT_LE(2u, frame_timestamps.size());

  // Iterate through the |frame_timestamps|, building a histogram counting the
  // number of times each frame was displayed k times.  For example, 10 frames
  // of 30 Hz content on a 60 Hz v-sync interval should result in
  // display_counts[2] == 10.  Quit early if any one frame was obviously
  // repeated too many times.
  const int64 max_expected_repeats_per_frame = 1 +
      std::max(GetParam().min_capture_period, GetParam().content_period) /
          GetParam().vsync_interval;
  std::vector<size_t> display_counts(max_expected_repeats_per_frame + 1, 0);
  base::TimeTicks last_present_time = frame_timestamps.front();
  for (Timestamps::const_iterator i = frame_timestamps.begin() + 1;
       i != frame_timestamps.end(); ++i) {
    const size_t num_vsync_intervals = static_cast<size_t>(
        (*i - last_present_time) / GetParam().vsync_interval);
    ASSERT_LT(0u, num_vsync_intervals);
    ASSERT_GT(display_counts.size(), num_vsync_intervals);  // Quit early.
    ++display_counts[num_vsync_intervals];
    last_present_time += num_vsync_intervals * GetParam().vsync_interval;
  }

  // Analyze the histogram for an expected result pattern.  If the frame
  // timestamps are smooth, there should only be one or two buckets with
  // non-zero counts and they should be next to each other.  Because the clock
  // precision for the event_times provided to the sampler is very granular
  // (i.e., the vsync_interval), it's okay if other buckets have a tiny "stray"
  // count in this test.
  size_t highest_count = 0;
  size_t second_highest_count = 0;
  for (size_t repeats = 1; repeats < display_counts.size(); ++repeats) {
    DVLOG(1) << "display_counts[" << repeats << "] is "
             << display_counts[repeats];
    if (display_counts[repeats] >= highest_count) {
      second_highest_count = highest_count;
      highest_count = display_counts[repeats];
    } else if (display_counts[repeats] > second_highest_count) {
      second_highest_count = display_counts[repeats];
    }
  }
  size_t stray_count_remaining =
      (frame_timestamps.size() - 1) - (highest_count + second_highest_count);
  // Expect no more than 0.75% of frames fall outside the two main buckets.
  EXPECT_GT(frame_timestamps.size() * 75 / 10000, stray_count_remaining);
  for (size_t repeats = 1; repeats < display_counts.size() - 1; ++repeats) {
    if (display_counts[repeats] == highest_count) {
      EXPECT_EQ(second_highest_count, display_counts[repeats + 1]);
      ++repeats;
    } else if (display_counts[repeats] == second_highest_count) {
      EXPECT_EQ(highest_count, display_counts[repeats + 1]);
      ++repeats;
    } else {
      EXPECT_GE(stray_count_remaining, display_counts[repeats]);
      stray_count_remaining -= display_counts[repeats];
    }
  }
}

// Tests that frame timestamps are "lightly pushed" back towards the original
// presentation event times, which tells us the AnimatedContentSampler can
// account for sources of timestamp drift and correct the drift.
TEST_P(AnimatedContentSamplerParameterizedTest,
       FrameTimestampsConvergeTowardsEventTimes) {
  const int max_drift_increment_millis = 3;

  // Generate a full minute of events.
  const base::TimeTicks begin = InitialTestTimeTicks();
  const base::TimeTicks end = begin + base::TimeDelta::FromMinutes(1);
  std::vector<Event> events = GenerateEventSequence(begin, end, true, false);

  // Modify the event sequence so that 1-3 ms of additional drift is suddenly
  // present every 100 events.  This is meant to simulate that, external to
  // AnimatedContentSampler, the video hardware vsync timebase is being
  // refreshed and is showing severe drift from the system clock.
  base::TimeDelta accumulated_drift;
  for (size_t i = 1; i < events.size(); ++i) {
    if (i % 100 == 0) {
      accumulated_drift += base::TimeDelta::FromMilliseconds(
          GetRandomInRange(1, max_drift_increment_millis + 1));
    }
    events[i].second += accumulated_drift;
  }

  // Run all the events through the sampler and track the last rewritten frame
  // timestamp.
  base::TimeTicks last_frame_timestamp;
  for (std::vector<Event>::const_iterator i = events.begin(); i != events.end();
       ++i) {
    sampler()->ConsiderPresentationEvent(i->first, i->second);
    if (sampler()->ShouldSample())
      last_frame_timestamp = sampler()->frame_timestamp();
  }

  // If drift was accounted for, the |last_frame_timestamp| should be close to
  // the last event's timestamp.
  const base::TimeDelta total_error =
      events.back().second - last_frame_timestamp;
  const base::TimeDelta max_acceptable_error = GetParam().min_capture_period +
      base::TimeDelta::FromMilliseconds(max_drift_increment_millis);
  EXPECT_NEAR(0.0,
              total_error.InMicroseconds(),
              max_acceptable_error.InMicroseconds());
}

INSTANTIATE_TEST_CASE_P(
    ,
    AnimatedContentSamplerParameterizedTest,
    ::testing::Values(
         // Typical frame rate content: Compositor runs at 60 Hz, capture at 30
         // Hz, and content video animates at 30, 25, or 24 Hz.
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(30)),
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(25)),
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(24)),

         // High frame rate content that leverages the Compositor's
         // capabilities, but capture is still at 30 Hz.
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(60)),
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(50)),
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(48)),

         // High frame rate content that leverages the Compositor's
         // capabilities, and capture is also a buttery 60 Hz.
         Scenario(FpsAsPeriod(60), FpsAsPeriod(60), FpsAsPeriod(60)),
         Scenario(FpsAsPeriod(60), FpsAsPeriod(60), FpsAsPeriod(50)),
         Scenario(FpsAsPeriod(60), FpsAsPeriod(60), FpsAsPeriod(48)),

         // On some platforms, the Compositor runs at 50 Hz.
         Scenario(FpsAsPeriod(50), FpsAsPeriod(30), FpsAsPeriod(30)),
         Scenario(FpsAsPeriod(50), FpsAsPeriod(30), FpsAsPeriod(25)),
         Scenario(FpsAsPeriod(50), FpsAsPeriod(30), FpsAsPeriod(24)),
         Scenario(FpsAsPeriod(50), FpsAsPeriod(30), FpsAsPeriod(50)),
         Scenario(FpsAsPeriod(50), FpsAsPeriod(30), FpsAsPeriod(48)),

         // Stable, but non-standard content frame rates.
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(16)),
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(20)),
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(23)),
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(26)),
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(27)),
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(28)),
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(29)),
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(31)),
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(32)),
         Scenario(FpsAsPeriod(60), FpsAsPeriod(30), FpsAsPeriod(33))));

// Tests that VideoCaptureOracle filters out events whose timestamps are
// decreasing.
TEST(VideoCaptureOracleTest, EnforcesEventTimeMonotonicity) {
  const base::TimeDelta min_capture_period =
      base::TimeDelta::FromSeconds(1) / 30;
  const gfx::Rect damage_rect(0, 0, 1280, 720);
  const base::TimeDelta event_increment = min_capture_period * 2;

  VideoCaptureOracle oracle(min_capture_period, true);

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

  VideoCaptureOracle oracle(min_capture_period, true);

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

  VideoCaptureOracle oracle(min_capture_period, true);

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
