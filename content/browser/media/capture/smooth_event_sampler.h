// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_SMOOTH_EVENT_SAMPLER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_SMOOTH_EVENT_SAMPLER_H_

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// Filters a sequence of events to achieve a target frequency.
class CONTENT_EXPORT SmoothEventSampler {
 public:
  SmoothEventSampler(base::TimeDelta min_capture_period,
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
  const base::TimeDelta min_capture_period_;
  const int redundant_capture_goal_;
  const base::TimeDelta token_bucket_capacity_;

  base::TimeTicks current_event_;
  base::TimeTicks last_sample_;
  int overdue_sample_count_;
  base::TimeDelta token_bucket_;

  DISALLOW_COPY_AND_ASSIGN(SmoothEventSampler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_SMOOTH_EVENT_SAMPLER_H_
