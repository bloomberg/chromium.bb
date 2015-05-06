// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_cadence_estimator.h"

#include <algorithm>
#include <limits>

#include "base/metrics/histogram_macros.h"

namespace media {

// To prevent oscillation in and out of cadence or between cadence values, we
// require some time to elapse before a cadence switch is accepted.
const int kMinimumCadenceDurationMs = 100;

// Records the number of cadence changes to UMA.
static void HistogramCadenceChangeCount(int cadence_changes) {
  const int kCadenceChangeMax = 10;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Media.VideoRenderer.CadenceChanges",
                              cadence_changes, 0, kCadenceChangeMax,
                              kCadenceChangeMax);
}

VideoCadenceEstimator::VideoCadenceEstimator(
    base::TimeDelta minimum_time_until_glitch)
    : cadence_hysteresis_threshold_(
          base::TimeDelta::FromMilliseconds(kMinimumCadenceDurationMs)),
      minimum_time_until_glitch_(minimum_time_until_glitch) {
  Reset();
}

VideoCadenceEstimator::~VideoCadenceEstimator() {
}

void VideoCadenceEstimator::Reset() {
  cadence_ = fractional_cadence_ = 0;
  pending_cadence_ = pending_fractional_cadence_ = 0;
  cadence_changes_ = render_intervals_cadence_held_ = 0;
  first_update_call_ = true;
}

bool VideoCadenceEstimator::UpdateCadenceEstimate(
    base::TimeDelta render_interval,
    base::TimeDelta frame_duration,
    base::TimeDelta max_acceptable_drift) {
  DCHECK_GT(render_interval, base::TimeDelta());
  DCHECK_GT(frame_duration, base::TimeDelta());

  base::TimeDelta time_until_cadence_glitch;
  base::TimeDelta time_until_fractional_cadence_glitch;

  // See if the clamped cadence fits acceptable thresholds for exhausting drift.
  int new_cadence = 0, new_fractional_cadence = 0;
  if (CalculateCadence(render_interval, frame_duration, max_acceptable_drift,
                       false, &new_cadence, &time_until_cadence_glitch)) {
    DCHECK(new_cadence);
  } else if (CalculateCadence(render_interval, frame_duration,
                              max_acceptable_drift, true,
                              &new_fractional_cadence,
                              &time_until_fractional_cadence_glitch)) {
    new_cadence = 1;
    DCHECK(new_fractional_cadence);
  }

  // If this is the first time UpdateCadenceEstimate() has been called,
  // initialize the histogram with a zero count for cadence changes; this
  // allows us to track the number of playbacks which have cadence at all.
  if (first_update_call_) {
    DCHECK_EQ(cadence_changes_, 0);
    first_update_call_ = false;
    HistogramCadenceChangeCount(0);
  }

  // Nothing changed, so do nothing.
  if (new_cadence == cadence_ &&
      new_fractional_cadence == fractional_cadence_) {
    // Clear cadence hold to pending values from accumulating incorrectly.
    render_intervals_cadence_held_ = 0;
    return false;
  }

  // Wait until enough render intervals have elapsed before accepting the
  // cadence change.  Prevents oscillation of the cadence selection.
  bool update_pending_cadence = true;
  if ((new_cadence == pending_cadence_ &&
       new_fractional_cadence == pending_fractional_cadence_) ||
      cadence_hysteresis_threshold_ <= render_interval) {
    if (++render_intervals_cadence_held_ * render_interval >=
        cadence_hysteresis_threshold_) {
      DVLOG(1) << "Cadence switch: (" << cadence_ << ", " << fractional_cadence_
               << ") -> (" << new_cadence << ", " << new_fractional_cadence
               << ") :: (" << time_until_cadence_glitch << ", "
               << time_until_fractional_cadence_glitch << ")";

      cadence_ = new_cadence;
      fractional_cadence_ = new_fractional_cadence;

      // Note: Because this class is transitively owned by a garbage collected
      // object, WebMediaPlayer, we log cadence changes as they are encountered.
      HistogramCadenceChangeCount(++cadence_changes_);
      return true;
    }

    update_pending_cadence = false;
  }

  DVLOG(2) << "Hysteresis prevented cadence switch: (" << cadence_ << ", "
           << fractional_cadence_ << ") -> (" << new_cadence << ", "
           << new_fractional_cadence << ") :: (" << time_until_cadence_glitch
           << ", " << time_until_fractional_cadence_glitch << ")";

  if (update_pending_cadence) {
    pending_cadence_ = new_cadence;
    pending_fractional_cadence_ = new_fractional_cadence;
    render_intervals_cadence_held_ = 1;
  }

  return false;
}

bool VideoCadenceEstimator::CalculateCadence(
    base::TimeDelta render_interval,
    base::TimeDelta frame_duration,
    base::TimeDelta max_acceptable_drift,
    bool fractional,
    int* cadence,
    base::TimeDelta* time_until_glitch) {
  // The perfect cadence is the number of render intervals per frame, while the
  // clamped cadence is the nearest matching integer cadence.
  //
  // Fractional cadence is checked to see if we have a cadence which would look
  // best if we consistently drop the same frames.
  //
  // As mentioned in the introduction, |perfect_cadence| is the ratio of the
  // frame duration to render interval length; while |clamped_cadence| is the
  // nearest integer value to |perfect_cadence|.  When computing a fractional
  // cadence (1/|perfect_cadence|), |fractional| must be set to true to ensure
  // the rendered and actual frame durations are computed correctly.
  const double perfect_cadence =
      fractional ? render_interval.InSecondsF() / frame_duration.InSecondsF()
                 : frame_duration.InSecondsF() / render_interval.InSecondsF();
  const int clamped_cadence = perfect_cadence + 0.5;
  if (!clamped_cadence)
    return false;

  // Calculate the drift in microseconds for each frame we render at cadence
  // instead of for its real duration.
  const base::TimeDelta rendered_frame_duration =
      fractional ? render_interval : clamped_cadence * render_interval;

  // When computing a fractional drift, we render the first of |clamped_cadence|
  // frames and drop |clamped_cadence| - 1 frames.  To make the calculations
  // below work we need to project out the timestamp of the frame which would be
  // rendered after accounting for those |clamped_cadence| frames.
  const base::TimeDelta actual_frame_duration =
      fractional ? clamped_cadence * frame_duration : frame_duration;
  if (rendered_frame_duration == actual_frame_duration) {
    *cadence = clamped_cadence;
    return true;
  }

  // Compute how long it'll take to exhaust the drift using |clamped_cadence|.
  const double duration_delta =
      (rendered_frame_duration - actual_frame_duration)
          .magnitude()
          .InMicroseconds();
  const int64 frames_until_drift_exhausted =
      std::ceil(max_acceptable_drift.InMicroseconds() / duration_delta);
  *time_until_glitch = rendered_frame_duration * frames_until_drift_exhausted;

  if (*time_until_glitch >= minimum_time_until_glitch_) {
    *cadence = clamped_cadence;
    return true;
  }

  return false;
}

int VideoCadenceEstimator::GetCadenceForFrame(int index) const {
  DCHECK(has_cadence());
  DCHECK_GE(index, 0);

  if (fractional_cadence_)
    return index % fractional_cadence_ == 0 ? 1 : 0;

  return cadence_;
}

}  // namespace media
