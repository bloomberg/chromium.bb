// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/video_capture_oracle.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace content {

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

// These specify the minimum/maximum amount of recent event history to examine
// to detect animated content.  If the values are too low, there is a greater
// risk of false-positive detections and low accuracy.  If they are too high,
// the the implementation will be slow to lock-in/out, and also will not react
// well to mildly-variable frame rate content (e.g., 25 +/- 1 FPS).
//
// These values were established by experimenting with a wide variety of
// scenarios, including 24/25/30 FPS videos, 60 FPS WebGL demos, and the
// transitions between static and animated content.
const int kMinObservationWindowMillis = 1000;
const int kMaxObservationWindowMillis = 2000;

// The maximum amount of time that can elapse before declaring two subsequent
// events as "not animating."  This is the same value found in
// cc::FrameRateCounter.
const int kNonAnimatingThresholdMillis = 250;  // 4 FPS

// The slowest that content can be animating in order for AnimatedContentSampler
// to lock-in.  This is the threshold at which the "smoothness" problem is no
// longer relevant.
const int kMaxLockInPeriodMicros = 83333;  // 12 FPS

// The amount of time over which to fully correct the drift of the rewritten
// frame timestamps from the presentation event timestamps.  The lower the
// value, the higher the variance in frame timestamps.
const int kDriftCorrectionMillis = 6000;

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

VideoCaptureOracle::VideoCaptureOracle(base::TimeDelta min_capture_period,
                                       bool events_are_reliable)
    : frame_number_(0),
      last_delivered_frame_number_(-1),
      smoothing_sampler_(min_capture_period,
                         events_are_reliable,
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

  bool should_sample;
  switch (event) {
    case kCompositorUpdate:
    case kSoftwarePaint:
      smoothing_sampler_.ConsiderPresentationEvent(event_time);
      content_sampler_.ConsiderPresentationEvent(damage_rect, event_time);
      if (content_sampler_.HasProposal()) {
        should_sample = content_sampler_.ShouldSample();
        if (should_sample)
          event_time = content_sampler_.frame_timestamp();
      } else {
        should_sample = smoothing_sampler_.ShouldSample();
      }
      break;
    default:
      should_sample = smoothing_sampler_.IsOverdueForSamplingAt(event_time);
      break;
  }

  SetFrameTimestamp(frame_number_, event_time);
  return should_sample;
}

int VideoCaptureOracle::RecordCapture() {
  smoothing_sampler_.RecordSample();
  content_sampler_.RecordSample(GetFrameTimestamp(frame_number_));
  return frame_number_++;
}

bool VideoCaptureOracle::CompleteCapture(int frame_number,
                                         base::TimeTicks* frame_timestamp) {
  // Drop frame if previous frame number is higher.
  if (last_delivered_frame_number_ > frame_number) {
    LOG(WARNING) << "Out of order frame delivery detected.  Dropping frame.";
    return false;
  }
  last_delivered_frame_number_ = frame_number;

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
  DCHECK_LE(frame_number, frame_number_);
  DCHECK_LT(frame_number_ - frame_number, kMaxFrameTimestamps);
  return frame_timestamps_[frame_number % kMaxFrameTimestamps];
}

void VideoCaptureOracle::SetFrameTimestamp(int frame_number,
                                           base::TimeTicks timestamp) {
  frame_timestamps_[frame_number % kMaxFrameTimestamps] = timestamp;
}

SmoothEventSampler::SmoothEventSampler(base::TimeDelta min_capture_period,
                                       bool events_are_reliable,
                                       int redundant_capture_goal)
    :  events_are_reliable_(events_are_reliable),
       min_capture_period_(min_capture_period),
       redundant_capture_goal_(redundant_capture_goal),
       token_bucket_capacity_(min_capture_period + min_capture_period / 2),
       overdue_sample_count_(0),
       token_bucket_(token_bucket_capacity_) {
  DCHECK_GT(min_capture_period_.InMicroseconds(), 0);
}

void SmoothEventSampler::ConsiderPresentationEvent(base::TimeTicks event_time) {
  DCHECK(!event_time.is_null());

  // Add tokens to the bucket based on advancement in time.  Then, re-bound the
  // number of tokens in the bucket.  Overflow occurs when there is too much
  // time between events (a common case), or when RecordSample() is not being
  // called often enough (a bug).  On the other hand, if RecordSample() is being
  // called too often (e.g., as a reaction to IsOverdueForSamplingAt()), the
  // bucket will underflow.
  if (!current_event_.is_null()) {
    if (current_event_ < event_time) {
      token_bucket_ += event_time - current_event_;
      if (token_bucket_ > token_bucket_capacity_)
        token_bucket_ = token_bucket_capacity_;
    }
    TRACE_COUNTER1("mirroring",
                   "MirroringTokenBucketUsec",
                   std::max<int64>(0, token_bucket_.InMicroseconds()));
  }
  current_event_ = event_time;
}

bool SmoothEventSampler::ShouldSample() const {
  return token_bucket_ >= min_capture_period_;
}

void SmoothEventSampler::RecordSample() {
  token_bucket_ -= min_capture_period_;
  if (token_bucket_ < base::TimeDelta())
    token_bucket_ = base::TimeDelta();
  TRACE_COUNTER1("mirroring",
                 "MirroringTokenBucketUsec",
                 std::max<int64>(0, token_bucket_.InMicroseconds()));

  if (HasUnrecordedEvent()) {
    last_sample_ = current_event_;
    overdue_sample_count_ = 0;
  } else {
    ++overdue_sample_count_;
  }
}

bool SmoothEventSampler::IsOverdueForSamplingAt(base::TimeTicks event_time)
    const {
  DCHECK(!event_time.is_null());

  // If we don't get events on compositor updates on this platform, then we
  // don't reliably know whether we're dirty.
  if (events_are_reliable_) {
    if (!HasUnrecordedEvent() &&
        overdue_sample_count_ >= redundant_capture_goal_) {
      return false;  // Not dirty.
    }
  }

  if (last_sample_.is_null())
    return true;

  // If we're dirty but not yet old, then we've recently gotten updates, so we
  // won't request a sample just yet.
  base::TimeDelta dirty_interval = event_time - last_sample_;
  return dirty_interval >=
      base::TimeDelta::FromMilliseconds(kNonAnimatingThresholdMillis);
}

bool SmoothEventSampler::HasUnrecordedEvent() const {
  return !current_event_.is_null() && current_event_ != last_sample_;
}

AnimatedContentSampler::AnimatedContentSampler(
    base::TimeDelta min_capture_period)
    : min_capture_period_(min_capture_period) {}

AnimatedContentSampler::~AnimatedContentSampler() {}

void AnimatedContentSampler::ConsiderPresentationEvent(
    const gfx::Rect& damage_rect, base::TimeTicks event_time) {
  AddObservation(damage_rect, event_time);

  if (AnalyzeObservations(event_time, &detected_region_, &detected_period_) &&
      detected_period_ > base::TimeDelta() &&
      detected_period_ <=
          base::TimeDelta::FromMicroseconds(kMaxLockInPeriodMicros)) {
    if (damage_rect == detected_region_)
      UpdateFrameTimestamp(event_time);
    else
      frame_timestamp_ = base::TimeTicks();
  } else {
    detected_region_ = gfx::Rect();
    detected_period_ = base::TimeDelta();
    frame_timestamp_ = base::TimeTicks();
  }
}

bool AnimatedContentSampler::HasProposal() const {
  return detected_period_ > base::TimeDelta();
}

bool AnimatedContentSampler::ShouldSample() const {
  return !frame_timestamp_.is_null();
}

void AnimatedContentSampler::RecordSample(base::TimeTicks frame_timestamp) {
  recorded_frame_timestamp_ = frame_timestamp;
  sequence_offset_ = base::TimeDelta();
}

void AnimatedContentSampler::AddObservation(const gfx::Rect& damage_rect,
                                            base::TimeTicks event_time) {
  if (damage_rect.IsEmpty())
    return;  // Useless observation.

  // Add the observation to the FIFO queue.
  if (!observations_.empty() && observations_.back().event_time > event_time)
    return;  // The implementation assumes chronological order.
  observations_.push_back(Observation(damage_rect, event_time));

  // Prune-out old observations.
  const base::TimeDelta threshold =
      base::TimeDelta::FromMilliseconds(kMaxObservationWindowMillis);
  while ((event_time - observations_.front().event_time) > threshold)
    observations_.pop_front();
}

gfx::Rect AnimatedContentSampler::ElectMajorityDamageRect() const {
  // This is an derivative of the Boyer-Moore Majority Vote Algorithm where each
  // pixel in a candidate gets one vote, as opposed to each candidate getting
  // one vote.
  const gfx::Rect* candidate = NULL;
  int64 votes = 0;
  for (ObservationFifo::const_iterator i = observations_.begin();
       i != observations_.end(); ++i) {
    DCHECK_GT(i->damage_rect.size().GetArea(), 0);
    if (votes == 0) {
      candidate = &(i->damage_rect);
      votes = candidate->size().GetArea();
    } else if (i->damage_rect == *candidate) {
      votes += i->damage_rect.size().GetArea();
    } else {
      votes -= i->damage_rect.size().GetArea();
      if (votes < 0) {
        candidate = &(i->damage_rect);
        votes = -votes;
      }
    }
  }
  return (votes > 0) ? *candidate : gfx::Rect();
}

bool AnimatedContentSampler::AnalyzeObservations(
    base::TimeTicks event_time,
    gfx::Rect* rect,
    base::TimeDelta* period) const {
  const gfx::Rect elected_rect = ElectMajorityDamageRect();
  if (elected_rect.IsEmpty())
    return false;  // There is no regular animation present.

  // Scan |observations_|, gathering metrics about the ones having a damage Rect
  // equivalent to the |elected_rect|.  Along the way, break early whenever the
  // event times reveal a non-animating period.
  int64 num_pixels_damaged_in_all = 0;
  int64 num_pixels_damaged_in_chosen = 0;
  base::TimeDelta sum_frame_durations;
  size_t count_frame_durations = 0;
  base::TimeTicks first_event_time;
  base::TimeTicks last_event_time;
  for (ObservationFifo::const_reverse_iterator i = observations_.rbegin();
       i != observations_.rend(); ++i) {
    const int area = i->damage_rect.size().GetArea();
    num_pixels_damaged_in_all += area;
    if (i->damage_rect != elected_rect)
      continue;
    num_pixels_damaged_in_chosen += area;
    if (last_event_time.is_null()) {
      last_event_time = i->event_time;
      if ((event_time - last_event_time) >=
              base::TimeDelta::FromMilliseconds(kNonAnimatingThresholdMillis)) {
        return false;  // Content animation has recently ended.
      }
    } else {
      const base::TimeDelta frame_duration = first_event_time - i->event_time;
      if (frame_duration >=
              base::TimeDelta::FromMilliseconds(kNonAnimatingThresholdMillis)) {
        break;  // Content not animating before this point.
      }
      sum_frame_durations += frame_duration;
      ++count_frame_durations;
    }
    first_event_time = i->event_time;
  }

  if ((last_event_time - first_event_time) <
          base::TimeDelta::FromMilliseconds(kMinObservationWindowMillis)) {
    return false;  // Content has not animated for long enough for accuracy.
  }
  if (num_pixels_damaged_in_chosen <= (num_pixels_damaged_in_all * 2 / 3))
    return false;  // Animation is not damaging a supermajority of pixels.

  *rect = elected_rect;
  DCHECK_GT(count_frame_durations, 0u);
  *period = sum_frame_durations / count_frame_durations;
  return true;
}

void AnimatedContentSampler::UpdateFrameTimestamp(base::TimeTicks event_time) {
  // This is how much time to advance from the last frame timestamp.  Never
  // advance by less than |min_capture_period_| because the downstream consumer
  // cannot handle the higher frame rate.  If |detected_period_| is less than
  // |min_capture_period_|, excess frames should be dropped.
  const base::TimeDelta advancement =
      std::max(detected_period_, min_capture_period_);

  // Compute the |timebase| upon which to determine the |frame_timestamp_|.
  // Ideally, this would always equal the timestamp of the last recorded frame
  // sampling.  Determine how much drift from the ideal is present, then adjust
  // the timebase by a small amount to spread out the entire correction over
  // many frame timestamps.
  //
  // This accounts for two main sources of drift: 1) The clock drift of the
  // system clock relative to the video hardware, which affects the event times;
  // and 2) The small error introduced by this frame timestamp rewriting, as it
  // is based on averaging over recent events.
  base::TimeTicks timebase = event_time - sequence_offset_ - advancement;
  if (!recorded_frame_timestamp_.is_null()) {
    const base::TimeDelta drift = recorded_frame_timestamp_ - timebase;
    const int64 correct_over_num_frames =
        base::TimeDelta::FromMilliseconds(kDriftCorrectionMillis) /
            detected_period_;
    DCHECK_GT(correct_over_num_frames, 0);
    timebase = recorded_frame_timestamp_ - (drift / correct_over_num_frames);
  }

  // Compute |frame_timestamp_|.  Whenever |detected_period_| is less than
  // |min_capture_period_|, some extra time is "borrowed" to be able to advance
  // by the full |min_capture_period_|.  Then, whenever the total amount of
  // borrowed time reaches a full |min_capture_period_|, drop a frame.  Note
  // that when |detected_period_| is greater or equal to |min_capture_period_|,
  // this logic is effectively disabled.
  borrowed_time_ += advancement - detected_period_;
  if (borrowed_time_ >= min_capture_period_) {
    borrowed_time_ -= min_capture_period_;
    frame_timestamp_ = base::TimeTicks();
  } else {
    sequence_offset_ += advancement;
    frame_timestamp_ = timebase + sequence_offset_;
  }
}

}  // namespace content
