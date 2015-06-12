// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_VIDEO_CAPTURE_ORACLE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_VIDEO_CAPTURE_ORACLE_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/browser/media/capture/animated_content_sampler.h"
#include "content/browser/media/capture/smooth_event_sampler.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

// VideoCaptureOracle manages the producer-side throttling of captured frames
// from a video capture device.  It is informed of every update by the device;
// this empowers it to look into the future and decide if a particular frame
// ought to be captured in order to achieve its target frame rate.
class CONTENT_EXPORT VideoCaptureOracle {
 public:
  enum Event {
    kTimerPoll,
    kCompositorUpdate,
    kNumEvents,
  };

  enum {
    // The recommended minimum amount of time between calls to
    // ObserveEventAndDecideCapture() for the kTimerPoll Event type.  Anything
    // lower than this isn't harmful, just wasteful.
    kMinTimerPollPeriodMillis = 125,  // 8 FPS
  };

  explicit VideoCaptureOracle(base::TimeDelta min_capture_period);
  virtual ~VideoCaptureOracle();

  // Record a event of type |event|, and decide whether the caller should do a
  // frame capture.  |damage_rect| is the region of a frame about to be drawn,
  // and may be an empty Rect, if this is not known.  If the caller accepts the
  // oracle's proposal, it should call RecordCapture() to indicate this.
  bool ObserveEventAndDecideCapture(Event event,
                                    const gfx::Rect& damage_rect,
                                    base::TimeTicks event_time);

  // Record the start of a capture.  Returns a frame_number to be used with
  // CompleteCapture().
  int RecordCapture();

  // Notify of the completion of a capture, and whether it was successful.
  // Returns true iff the captured frame should be delivered.  |frame_timestamp|
  // is set to the timestamp that should be provided to the consumer of the
  // frame.
  bool CompleteCapture(int frame_number,
                       bool capture_was_successful,
                       base::TimeTicks* frame_timestamp);

  base::TimeDelta min_capture_period() const {
    return smoothing_sampler_.min_capture_period();
  }

  // Returns the oracle's estimate of the duration of the next frame.  This
  // should be called just after ObserveEventAndDecideCapture(), and will only
  // be non-zero if the call returned true.
  base::TimeDelta estimated_frame_duration() const {
    return duration_of_next_frame_;
  }

 private:
  // Retrieve/Assign a frame timestamp by capture |frame_number|.
  base::TimeTicks GetFrameTimestamp(int frame_number) const;
  void SetFrameTimestamp(int frame_number, base::TimeTicks timestamp);

  // Incremented every time a paint or update event occurs.
  int next_frame_number_;

  // Stores the last |event_time| from the last observation/decision.  Used to
  // sanity-check that event times are monotonically non-decreasing.
  base::TimeTicks last_event_time_[kNumEvents];

  // Updated by the last call to ObserveEventAndDecideCapture() with the
  // estimated duration of the next frame to sample.  This is zero if the method
  // returned false.
  base::TimeDelta duration_of_next_frame_;

  // Stores the frame number from the last successfully delivered frame.
  int last_successfully_delivered_frame_number_;

  // Stores the number of pending frame captures.
  int num_frames_pending_;

  // These track present/paint history and propose whether to sample each event
  // for capture.  |smoothing_sampler_| uses a "works for all" heuristic, while
  // |content_sampler_| specifically detects animated content (e.g., video
  // playback) and decides which events to sample to "lock into" that content.
  SmoothEventSampler smoothing_sampler_;
  AnimatedContentSampler content_sampler_;

  // Recent history of frame timestamps proposed by VideoCaptureOracle.  This is
  // a ring-buffer, and should only be accessed by the Get/SetFrameTimestamp()
  // methods.
  enum { kMaxFrameTimestamps = 16 };
  base::TimeTicks frame_timestamps_[kMaxFrameTimestamps];
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_VIDEO_CAPTURE_ORACLE_H_
