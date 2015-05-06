// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_CADENCE_ESTIMATOR_H_
#define MEDIA_FILTERS_VIDEO_CADENCE_ESTIMATOR_H_

#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

// Estimates whether a given frame duration and render interval length have a
// render cadence which would allow for optimal uniformity of displayed frame
// durations over time.
//
// Cadence is the ratio of the frame duration to render interval length.  I.e.
// for 30fps in 60Hz the cadence would be (1/30) / (1/60) = 60 / 30 = 2.  It's
// common that this is not an exact integer, e.g., 29.974fps in 60Hz which
// would have a cadence of (1/29.974) / (1/60) = ~2.0029.
//
// Clamped integer cadence means we round the actual cadence (~2.0029 in the
// pending example) to the nearest integer value (2 in this case).  If the
// delta between those values is small, we can choose to render frames for the
// integer number of render intervals; shortening or lengthening the actual
// rendered frame duration.  Doing so ensures each frame gets an optimal amount
// of display time.
//
// Obviously clamping cadence like that leads to drift over time of the actual
// VideoFrame timestamp relative to its rendered time, so we perform some
// calculations to ensure we only clamp cadence when it will take some time to
// drift an undesirable amount; see CalculateCadence() for details on how this
// calculation is made.
//
// Notably, this concept can be extended to include fractional cadence when the
// frame duration is shorter than the render interval; e.g. 120fps in 60Hz.  In
// this case, the first frame in each group of N frames is displayed once, while
// the next N - 1 frames are dropped; where N is the fractional cadence of the
// frame group.  Using the pending example N = 120/60 = 2. See implementations
// of CalculateCadence() and UpdateCadenceEstimate() for more details.
//
// In practice this works out to the following for common setups if we use
// cadence based selection:
//
//    29.5fps in 60Hz,    ~17ms max drift => exhausted in ~1 second.
//    29.9fps in 60Hz,    ~17ms max drift => exhausted in ~16.4 seconds.
//    24fps   in 60Hz,    ~21ms max drift => exhausted in ~0.15 seconds.
//    25fps   in 60Hz,     20ms max drift => exhausted in ~4.0 seconds.
//    59.9fps in 60Hz,   ~8.3ms max drift => exhausted in ~8.2 seconds.
//    24.9fps in 50Hz,    ~20ms max drift => exhausted in ~20.5 seconds.
//    120fps  in 59.9Hz, ~8.3ms max drift => exhausted in ~8.2 seconds.
//
class MEDIA_EXPORT VideoCadenceEstimator {
 public:
  // As mentioned in the introduction, the determination of whether to clamp to
  // a given cadence is based on how long it takes before a frame would have to
  // be dropped or repeated to compensate for reaching the maximum acceptable
  // drift; this time length is controlled by |minimum_time_until_glitch|.
  explicit VideoCadenceEstimator(base::TimeDelta minimum_time_until_glitch);
  ~VideoCadenceEstimator();

  // Clears stored cadence information.
  void Reset();

  // Updates the estimates for |cadence_| and |fractional_cadence_| based on the
  // given values as described in the introduction above.
  //
  // Clients should call this and then update the cadence for all frames via the
  // GetCadenceForFrame() method.
  //
  // Cadence changes will not take affect until enough render intervals have
  // elapsed.  For the purposes of hysteresis, each UpdateCadenceEstimate() call
  // is assumed to elapse one |render_interval| worth of time.
  //
  // Returns true if the cadence has changed since the last call.
  bool UpdateCadenceEstimate(base::TimeDelta render_interval,
                             base::TimeDelta frame_duration,
                             base::TimeDelta max_acceptable_drift);

  // Returns true if a useful cadence was found.
  bool has_cadence() const { return cadence_ > 0; }

  // Given a frame |index|, where zero is the most recently rendered frame,
  // returns the ideal cadence for that frame.
  int GetCadenceForFrame(int index) const;

  void set_cadence_hysteresis_threshold_for_testing(base::TimeDelta threshold) {
    cadence_hysteresis_threshold_ = threshold;
  }

  int get_cadence_for_testing() const {
    return cadence_ && fractional_cadence_ ? fractional_cadence_ : cadence_;
  }

 private:
  // Calculates the clamped cadence for the given |render_interval| and
  // |frame_duration|, then calculates how long that cadence can be used before
  // exhausting |max_acceptable_drift|.  If the time until exhaustion is greater
  // than |minimum_time_until_glitch_|, returns true and sets |cadence| to the
  // clamped cadence.  If the clamped cadence is unusable, |cadence| will be set
  // to zero.
  //
  // If |fractional| is true, GetCadence() will calculate the clamped cadence
  // using the ratio of the |render_interval| to |frame_duration| instead of
  // vice versa.
  //
  // Sets |time_until_glitch| to the computed glitch time.  Set to zero if the
  // clamped cadence is unusable.
  bool CalculateCadence(base::TimeDelta render_interval,
                        base::TimeDelta frame_duration,
                        base::TimeDelta max_acceptable_drift,
                        bool fractional,
                        int* cadence,
                        base::TimeDelta* time_until_glitch);

  // The idealized cadence for all frames seen thus far; updated based upon the
  // ratio of |frame_duration| to |render_interval|, or vice versa, as given to
  // UpdateCadenceEstimate().  Zero if no integer cadence could be detected.
  //
  // Fractional cadences are handled by strongly preferring the first frame in
  // a series if it fits within acceptable drift. E.g., with 120fps content on
  // a 60Hz monitor we'll strongly prefer the first frame of every 2 frames.
  //
  // |fractional_cadence_| is the number of frames per render interval; the
  // first of which would be rendered and the rest dropped.
  int cadence_;
  int fractional_cadence_;

  // Used as hysteresis to prevent oscillation between cadence and coverage
  // based rendering methods.  Pending values are updated upon each new cadence
  // detected by UpdateCadenceEstimate().
  //
  // Once a new cadence is detected, |render_intervals_cadence_held_| is
  // incremented for each UpdateCadenceEstimate() call where the cadence matches
  // one of the pending values.  |render_intervals_cadence_held_| is cleared
  // when a "new" cadence matches |cadence_| or |pending_cadence_|.
  //
  // Once |kMinimumCadenceDurationMs| is exceeded in render intervals, the
  // detected cadence is set in |cadence_| and |fractional_cadence_|.
  int pending_cadence_;
  int pending_fractional_cadence_;
  int render_intervals_cadence_held_;
  base::TimeDelta cadence_hysteresis_threshold_;

  // Tracks how many times cadence has switched during a given playback, used to
  // histogram the number of cadence changes in a playback.
  bool first_update_call_;
  int cadence_changes_;

  // The minimum amount of time allowed before a glitch occurs before confirming
  // cadence for a given render interval and frame duration.
  const base::TimeDelta minimum_time_until_glitch_;

  DISALLOW_COPY_AND_ASSIGN(VideoCadenceEstimator);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_CADENCE_ESTIMATOR_H_
