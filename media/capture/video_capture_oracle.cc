// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video_capture_oracle.h"

#include <algorithm>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace media {

namespace {

// This value controls how many redundant, timer-base captures occur when the
// content is static. Redundantly capturing the same frame allows iterative
// quality enhancement, and also allows the buffer to fill in "buffered mode".
//
// TODO(nick): Controlling this here is a hack and a layering violation, since
// it's a strategy specific to the WebRTC consumer, and probably just papers
// over some frame dropping and quality bugs. It should either be controlled at
// a higher level, or else redundant frame generation should be pushed down
// further into the WebRTC encoding stack.
const int kNumRedundantCapturesOfStaticContent = 200;

// Given the amount of time between frames, compare to the expected amount of
// time between frames at |frame_rate| and return the fractional difference.
double FractionFromExpectedFrameRate(base::TimeDelta delta, int frame_rate) {
  DCHECK_GT(frame_rate, 0);
  const base::TimeDelta expected_delta =
      base::TimeDelta::FromSeconds(1) / frame_rate;
  return (delta - expected_delta).InMillisecondsF() /
      expected_delta.InMillisecondsF();
}

}  // anonymous namespace

VideoCaptureOracle::VideoCaptureOracle(base::TimeDelta min_capture_period)
    : next_frame_number_(0),
      last_successfully_delivered_frame_number_(-1),
      num_frames_pending_(0),
      smoothing_sampler_(min_capture_period,
                         kNumRedundantCapturesOfStaticContent),
      content_sampler_(min_capture_period) {
}

VideoCaptureOracle::~VideoCaptureOracle() {}

bool VideoCaptureOracle::ObserveEventAndDecideCapture(
    Event event,
    const gfx::Rect& damage_rect,
    base::TimeTicks event_time) {
  DCHECK_GE(event, 0);
  DCHECK_LT(event, kNumEvents);
  if (event_time < last_event_time_[event]) {
    LOG(WARNING) << "Event time is not monotonically non-decreasing.  "
                 << "Deciding not to capture this frame.";
    return false;
  }
  last_event_time_[event] = event_time;

  bool should_sample = false;
  duration_of_next_frame_ = base::TimeDelta();
  switch (event) {
    case kCompositorUpdate:
      smoothing_sampler_.ConsiderPresentationEvent(event_time);
      content_sampler_.ConsiderPresentationEvent(damage_rect, event_time);
      if (content_sampler_.HasProposal()) {
        should_sample = content_sampler_.ShouldSample();
        if (should_sample) {
          event_time = content_sampler_.frame_timestamp();
          duration_of_next_frame_ = content_sampler_.sampling_period();
        }
      } else {
        should_sample = smoothing_sampler_.ShouldSample();
        if (should_sample)
          duration_of_next_frame_ = smoothing_sampler_.min_capture_period();
      }
      break;
    case kTimerPoll:
      // While the timer is firing, only allow a sampling if there are none
      // currently in-progress.
      if (num_frames_pending_ == 0) {
        should_sample = smoothing_sampler_.IsOverdueForSamplingAt(event_time);
        if (should_sample)
          duration_of_next_frame_ = smoothing_sampler_.min_capture_period();
      }
      break;
    case kNumEvents:
      NOTREACHED();
      break;
  }

  SetFrameTimestamp(next_frame_number_, event_time);
  return should_sample;
}

int VideoCaptureOracle::RecordCapture() {
  smoothing_sampler_.RecordSample();
  content_sampler_.RecordSample(GetFrameTimestamp(next_frame_number_));
  num_frames_pending_++;
  return next_frame_number_++;
}

bool VideoCaptureOracle::CompleteCapture(int frame_number,
                                         bool capture_was_successful,
                                         base::TimeTicks* frame_timestamp) {
  num_frames_pending_--;

  // Drop frame if previously delivered frame number is higher.
  if (last_successfully_delivered_frame_number_ > frame_number) {
    LOG_IF(WARNING, capture_was_successful)
        << "Out of order frame delivery detected (have #" << frame_number
        << ", last was #" << last_successfully_delivered_frame_number_
        << ").  Dropping frame.";
    return false;
  }

  if (!capture_was_successful) {
    VLOG(2) << "Capture of frame #" << frame_number << " was not successful.";
    return false;
  }

  DCHECK_NE(last_successfully_delivered_frame_number_, frame_number);
  last_successfully_delivered_frame_number_ = frame_number;

  *frame_timestamp = GetFrameTimestamp(frame_number);

  // If enabled, log a measurement of how this frame timestamp has incremented
  // in relation to an ideal increment.
  if (VLOG_IS_ON(2) && frame_number > 0) {
    const base::TimeDelta delta =
        *frame_timestamp - GetFrameTimestamp(frame_number - 1);
    if (content_sampler_.HasProposal()) {
      const double estimated_frame_rate =
          1000000.0 / content_sampler_.detected_period().InMicroseconds();
      const int rounded_frame_rate =
          static_cast<int>(estimated_frame_rate + 0.5);
      VLOG(2) << base::StringPrintf(
          "Captured #%d: delta=%" PRId64 " usec"
          ", now locked into {%s}, %+0.1f%% slower than %d FPS",
          frame_number,
          delta.InMicroseconds(),
          content_sampler_.detected_region().ToString().c_str(),
          100.0 * FractionFromExpectedFrameRate(delta, rounded_frame_rate),
          rounded_frame_rate);
    } else {
      VLOG(2) << base::StringPrintf(
          "Captured #%d: delta=%" PRId64 " usec"
          ", d/30fps=%+0.1f%%, d/25fps=%+0.1f%%, d/24fps=%+0.1f%%",
          frame_number,
          delta.InMicroseconds(),
          100.0 * FractionFromExpectedFrameRate(delta, 30),
          100.0 * FractionFromExpectedFrameRate(delta, 25),
          100.0 * FractionFromExpectedFrameRate(delta, 24));
    }
  }

  return !frame_timestamp->is_null();
}

base::TimeTicks VideoCaptureOracle::GetFrameTimestamp(int frame_number) const {
  DCHECK_LE(frame_number, next_frame_number_);
  DCHECK_LT(next_frame_number_ - frame_number, kMaxFrameTimestamps);
  return frame_timestamps_[frame_number % kMaxFrameTimestamps];
}

void VideoCaptureOracle::SetFrameTimestamp(int frame_number,
                                           base::TimeTicks timestamp) {
  frame_timestamps_[frame_number % kMaxFrameTimestamps] = timestamp;
}

}  // namespace media
