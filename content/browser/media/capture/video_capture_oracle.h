// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_VIDEO_CAPTURE_ORACLE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_VIDEO_CAPTURE_ORACLE_H_

#include <deque>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

// Filters a sequence of events to achieve a target frequency.
class CONTENT_EXPORT SmoothEventSampler {
 public:
  SmoothEventSampler(base::TimeDelta min_capture_period,
                     bool events_are_reliable,
                     int redundant_capture_goal);

  base::TimeDelta min_capture_period() const { return min_capture_period_; }

  // Add a new event to the event history, and consider whether it ought to be
  // sampled. The event is not recorded as a sample until RecordSample() is
  // called.
  void ConsiderPresentationEvent(base::TimeTicks event_time);

  // Returns true if the last event considered should be sampled.
  bool ShouldSample() const;

  // Operates on the last event added by ConsiderPresentationEvent(), marking
  // it as sampled. After this point we are current in the stream of events, as
  // we have sampled the most recent event.
  void RecordSample();

  // Returns true if, at time |event_time|, sampling should occur because too
  // much time will have passed relative to the last event and/or sample.
  bool IsOverdueForSamplingAt(base::TimeTicks event_time) const;

  // Returns true if ConsiderPresentationEvent() has been called since the last
  // call to RecordSample().
  bool HasUnrecordedEvent() const;

 private:
  const bool events_are_reliable_;
  const base::TimeDelta min_capture_period_;
  const int redundant_capture_goal_;
  const base::TimeDelta token_bucket_capacity_;

  base::TimeTicks current_event_;
  base::TimeTicks last_sample_;
  int overdue_sample_count_;
  base::TimeDelta token_bucket_;

  DISALLOW_COPY_AND_ASSIGN(SmoothEventSampler);
};

// Analyzes a sequence of events to detect the presence of constant frame rate
// animated content.  In the case where there are multiple regions of animated
// content, AnimatedContentSampler will propose sampling the one having the
// largest "smoothness" impact, according to human perception (e.g., a 24 FPS
// video versus a 60 FPS busy spinner).
//
// In addition, AnimatedContentSampler will provide rewritten frame timestamps,
// for downstream consumers, that are "truer" to the source content than to the
// local presentation hardware.
class CONTENT_EXPORT AnimatedContentSampler {
 public:
  explicit AnimatedContentSampler(base::TimeDelta min_capture_period);
  ~AnimatedContentSampler();

  // Examines the given presentation event metadata, along with recent history,
  // to detect animated content, updating the state of this sampler.
  // |damage_rect| is the region of a frame about to be drawn, while
  // |event_time| refers to the frame's estimated presentation time.
  void ConsiderPresentationEvent(const gfx::Rect& damage_rect,
                                 base::TimeTicks event_time);

  // Returns true if animated content has been detected and a decision has been
  // made about whether to sample the last event.
  bool HasProposal() const;

  // Returns true if the last event considered should be sampled.
  bool ShouldSample() const;

  // Returns a frame timestamp to provide to consumers of the sampled frame.
  // Only valid when should_sample() returns true.
  base::TimeTicks frame_timestamp() const { return frame_timestamp_; }

  // Accessors to currently-detected animating region/period, for logging.
  const gfx::Rect& detected_region() const { return detected_region_; }
  base::TimeDelta detected_period() const { return detected_period_; }

  // Records that a frame with the given |frame_timestamp| was sampled.  This
  // method should be called when *any* sampling is taken, even if it was not
  // proposed by AnimatedContentSampler.
  void RecordSample(base::TimeTicks frame_timestamp);

 private:
  friend class AnimatedContentSamplerTest;

  // Data structure for efficient online analysis of recent event history.
  struct Observation {
    gfx::Rect damage_rect;
    base::TimeTicks event_time;

    Observation(const gfx::Rect& d, base::TimeTicks e)
        : damage_rect(d), event_time(e) {}
  };
  typedef std::deque<Observation> ObservationFifo;

  // Adds an observation to |observations_|, and prunes-out the old ones.
  void AddObservation(const gfx::Rect& damage_rect, base::TimeTicks event_time);

  // Returns the damage Rect that is responsible for the majority of the pixel
  // damage in recent event history, if there is such a Rect.  If there isn't,
  // this method could still return any Rect, so the caller must confirm the
  // returned Rect really is responsible for the majority of pixel damage.
  gfx::Rect ElectMajorityDamageRect() const;

  // Analyzes the observations relative to the current |event_time| to detect
  // stable animating content.  If detected, returns true and sets the output
  // arguments to the region of the animating content and its mean frame
  // duration.
  bool AnalyzeObservations(base::TimeTicks event_time,
                           gfx::Rect* rect,
                           base::TimeDelta* period) const;

  // Called by ConsiderPresentationEvent() when the current event is part of a
  // detected animation, to update |frame_timestamp_|.
  void UpdateFrameTimestamp(base::TimeTicks event_time);

  // The client expects frame timestamps to be at least this far apart.
  const base::TimeDelta min_capture_period_;

  // A recent history of observations in chronological order, maintained by
  // AddObservation().
  ObservationFifo observations_;

  // The region of currently-detected animated content.  If empty, that means
  // "not detected."
  gfx::Rect detected_region_;

  // The mean frame duration of currently-detected animated content.  If zero,
  // that means "not detected."
  base::TimeDelta detected_period_;

  // The rewritten frame timestamp for the latest event.
  base::TimeTicks frame_timestamp_;

  // The frame timestamp provided in the last call to RecordSample().  This
  // timestamp may or may not have been one proposed by AnimatedContentSampler.
  base::TimeTicks recorded_frame_timestamp_;

  // Accumulates all the time advancements since the last call to
  // RecordSample().  When this is greater than zero, there have been one or
  // more events proposed for sampling, but not yet recorded.  This accounts for
  // the cases where AnimatedContentSampler indicates a frame should be sampled,
  // but the client chooses not to do so.
  base::TimeDelta sequence_offset_;

  // A token bucket that is used to decide which frames to drop whenever
  // |detected_period_| is less than |min_capture_period_|.
  base::TimeDelta borrowed_time_;
};

// VideoCaptureOracle manages the producer-side throttling of captured frames
// from a video capture device.  It is informed of every update by the device;
// this empowers it to look into the future and decide if a particular frame
// ought to be captured in order to achieve its target frame rate.
class CONTENT_EXPORT VideoCaptureOracle {
 public:
  enum Event {
    kTimerPoll,
    kCompositorUpdate,
    kSoftwarePaint,
    kNumEvents,
  };

  VideoCaptureOracle(base::TimeDelta min_capture_period,
                     bool events_are_reliable);
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

  // Notify of the completion of a capture.  Returns true iff the captured frame
  // should be delivered.  |frame_timestamp| is set to the timestamp that should
  // be provided to the consumer of the frame.
  bool CompleteCapture(int frame_number, base::TimeTicks* frame_timestamp);

  base::TimeDelta min_capture_period() const {
    return smoothing_sampler_.min_capture_period();
  }

 private:
  // Retrieve/Assign a frame timestamp by capture |frame_number|.
  base::TimeTicks GetFrameTimestamp(int frame_number) const;
  void SetFrameTimestamp(int frame_number, base::TimeTicks timestamp);

  // Incremented every time a paint or update event occurs.
  int frame_number_;

  // Stores the last |event_time| from the last observation/decision.  Used to
  // sanity-check that event times are monotonically non-decreasing.
  base::TimeTicks last_event_time_[kNumEvents];

  // Stores the frame number from the last delivered frame.
  int last_delivered_frame_number_;

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
