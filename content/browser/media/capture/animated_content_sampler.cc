// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/animated_content_sampler.h"

#include <algorithm>

namespace content {

namespace {

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
const int kDriftCorrectionMillis = 2000;

}  // anonymous namespace

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
  recorded_frame_timestamp_ =
      HasProposal() ? frame_timestamp : base::TimeTicks();
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
