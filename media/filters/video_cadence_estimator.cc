// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_cadence_estimator.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <string>

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
    base::TimeDelta minimum_time_until_max_drift)
    : cadence_hysteresis_threshold_(
          base::TimeDelta::FromMilliseconds(kMinimumCadenceDurationMs)),
      minimum_time_until_max_drift_(minimum_time_until_max_drift) {
  Reset();
}

VideoCadenceEstimator::~VideoCadenceEstimator() {
}

void VideoCadenceEstimator::Reset() {
  cadence_.clear();
  pending_cadence_.clear();
  cadence_changes_ = render_intervals_cadence_held_ = 0;
  first_update_call_ = true;
}

bool VideoCadenceEstimator::UpdateCadenceEstimate(
    base::TimeDelta render_interval,
    base::TimeDelta frame_duration,
    base::TimeDelta max_acceptable_drift) {
  DCHECK_GT(render_interval, base::TimeDelta());
  DCHECK_GT(frame_duration, base::TimeDelta());

  base::TimeDelta time_until_max_drift;

  // See if we can find a cadence which fits the data.
  Cadence new_cadence =
      CalculateCadence(render_interval, frame_duration, max_acceptable_drift,
                       &time_until_max_drift);

  // If this is the first time UpdateCadenceEstimate() has been called,
  // initialize the histogram with a zero count for cadence changes; this
  // allows us to track the number of playbacks which have cadence at all.
  if (first_update_call_) {
    DCHECK_EQ(cadence_changes_, 0);
    first_update_call_ = false;
    HistogramCadenceChangeCount(0);
  }

  // If nothing changed, do nothing.
  if (new_cadence == cadence_) {
    // Clear cadence hold to pending values from accumulating incorrectly.
    render_intervals_cadence_held_ = 0;
    return false;
  }

  // Wait until enough render intervals have elapsed before accepting the
  // cadence change.  Prevents oscillation of the cadence selection.
  bool update_pending_cadence = true;
  if (new_cadence == pending_cadence_ ||
      cadence_hysteresis_threshold_ <= render_interval) {
    if (++render_intervals_cadence_held_ * render_interval >=
        cadence_hysteresis_threshold_) {
      DVLOG(1) << "Cadence switch: " << CadenceToString(cadence_) << " -> "
               << CadenceToString(new_cadence)
               << " :: Time until drift exceeded: " << time_until_max_drift;
      cadence_.swap(new_cadence);

      // Note: Because this class is transitively owned by a garbage collected
      // object, WebMediaPlayer, we log cadence changes as they are encountered.
      HistogramCadenceChangeCount(++cadence_changes_);
      return true;
    }

    update_pending_cadence = false;
  }

  DVLOG(2) << "Hysteresis prevented cadence switch: "
           << CadenceToString(cadence_) << " -> "
           << CadenceToString(new_cadence);

  if (update_pending_cadence) {
    pending_cadence_.swap(new_cadence);
    render_intervals_cadence_held_ = 1;
  }

  return false;
}

int VideoCadenceEstimator::GetCadenceForFrame(uint64_t frame_number) const {
  DCHECK(has_cadence());
  return cadence_[frame_number % cadence_.size()];
}

VideoCadenceEstimator::Cadence VideoCadenceEstimator::CalculateCadence(
    base::TimeDelta render_interval,
    base::TimeDelta frame_duration,
    base::TimeDelta max_acceptable_drift,
    base::TimeDelta* time_until_max_drift) const {
  // See if we can find a cadence which fits the data.
  Cadence result;
  if (CalculateOneFrameCadence(render_interval, frame_duration,
                               max_acceptable_drift, &result,
                               time_until_max_drift)) {
    DCHECK_EQ(1u, result.size());
  } else if (CalculateFractionalCadence(render_interval, frame_duration,
                                        max_acceptable_drift, &result,
                                        time_until_max_drift)) {
    DCHECK(!result.empty());
  } else if (CalculateOneFrameCadence(render_interval, frame_duration * 2,
                                      max_acceptable_drift, &result,
                                      time_until_max_drift)) {
    // By finding cadence for double the frame duration, we're saying there
    // exist two integers a and b, where a > b and a + b = |result|; this
    // matches all patterns which regularly have half a frame per render
    // interval; i.e. 24fps in 60hz.
    DCHECK_EQ(1u, result.size());

    // While we may find a two pattern cadence, sometimes one extra frame
    // duration is enough to allow a match for 1-frame cadence if the
    // |time_until_max_drift| was on the edge.
    //
    // All 2-frame cadence values should be odd, so we can detect this and fall
    // back to 1-frame cadence when this occurs.
    if (result[0] & 1) {
      result[0] = std::ceil(result[0] / 2.0);
      result.push_back(result[0] - 1);
    } else {
      result[0] /= 2;
    }
  }
  return result;
}

bool VideoCadenceEstimator::CalculateOneFrameCadence(
    base::TimeDelta render_interval,
    base::TimeDelta frame_duration,
    base::TimeDelta max_acceptable_drift,
    Cadence* cadence,
    base::TimeDelta* time_until_max_drift) const {
  DCHECK(cadence->empty());

  // The perfect cadence is the number of render intervals per frame, while the
  // clamped cadence is the nearest matching integer value.
  //
  // As mentioned in the introduction, |perfect_cadence| is the ratio of the
  // frame duration to render interval length; while |clamped_cadence| is the
  // nearest integer value to |perfect_cadence|.
  const double perfect_cadence =
      frame_duration.InSecondsF() / render_interval.InSecondsF();
  const int clamped_cadence = perfect_cadence + 0.5;
  if (!clamped_cadence)
    return false;

  // For cadence based rendering the actual frame duration is just the frame
  // duration, while the |rendered_frame_duration| is how long the frame would
  // be displayed for if we rendered it |clamped_cadence| times.
  const base::TimeDelta rendered_frame_duration =
      clamped_cadence * render_interval;
  if (!IsAcceptableCadence(rendered_frame_duration, frame_duration,
                           max_acceptable_drift, time_until_max_drift)) {
    return false;
  }

  cadence->push_back(clamped_cadence);
  return true;
}

bool VideoCadenceEstimator::CalculateFractionalCadence(
    base::TimeDelta render_interval,
    base::TimeDelta frame_duration,
    base::TimeDelta max_acceptable_drift,
    Cadence* cadence,
    base::TimeDelta* time_until_max_drift) const {
  DCHECK(cadence->empty());

  // Fractional cadence allows us to see if we have a cadence which would look
  // best if we consistently drop the same frames.
  //
  // In this case, the perfect cadence is the number of frames per render
  // interval, while the clamped cadence is the nearest integer value.
  const double perfect_cadence =
      render_interval.InSecondsF() / frame_duration.InSecondsF();
  const int clamped_cadence = perfect_cadence + 0.5;
  if (!clamped_cadence)
    return false;

  // For fractional cadence, the rendered duration of each frame is just the
  // render interval.  While the actual frame duration is the total duration of
  // all the frames we would end up dropping during that time.
  const base::TimeDelta actual_frame_duration =
      clamped_cadence * frame_duration;
  if (!IsAcceptableCadence(render_interval, actual_frame_duration,
                           max_acceptable_drift, time_until_max_drift)) {
    return false;
  }

  // Fractional cadence means we render the first of |clamped_cadence| frames
  // and drop |clamped_cadence| - 1 frames.
  cadence->insert(cadence->begin(), clamped_cadence, 0);
  (*cadence)[0] = 1;
  return true;
}

std::string VideoCadenceEstimator::CadenceToString(
    const Cadence& cadence) const {
  if (cadence.empty())
    return std::string("[]");

  std::ostringstream os;
  os << "[";
  std::copy(cadence.begin(), cadence.end() - 1,
            std::ostream_iterator<int>(os, ":"));
  os << cadence.back() << "]";
  return os.str();
}

bool VideoCadenceEstimator::IsAcceptableCadence(
    base::TimeDelta rendered_frame_duration,
    base::TimeDelta actual_frame_duration,
    base::TimeDelta max_acceptable_drift,
    base::TimeDelta* time_until_max_drift) const {
  if (rendered_frame_duration == actual_frame_duration)
    return true;

  // Compute how long it'll take to exhaust the drift if frames are rendered for
  // |rendered_frame_duration| instead of |actual_frame_duration|.
  const double duration_delta =
      (rendered_frame_duration - actual_frame_duration)
          .magnitude()
          .InMicroseconds();
  const int64 frames_until_drift_exhausted =
      std::ceil(max_acceptable_drift.InMicroseconds() / duration_delta);

  // If the time until a frame would be repeated or dropped is greater than our
  // limit of acceptability, the cadence is acceptable.
  *time_until_max_drift =
      rendered_frame_duration * frames_until_drift_exhausted;
  return *time_until_max_drift >= minimum_time_until_max_drift_;
}

}  // namespace media
