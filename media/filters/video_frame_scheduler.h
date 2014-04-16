// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_FRAME_SCHEDULER_H_
#define MEDIA_FILTERS_VIDEO_FRAME_SCHEDULER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"

namespace media {

class VideoFrame;

// Defines an abstract video frame scheduler that is capable of managing the
// display of video frames at explicit times.
class MEDIA_EXPORT VideoFrameScheduler {
 public:
  VideoFrameScheduler() {}
  virtual ~VideoFrameScheduler() {}

  enum Reason {
    DISPLAYED,  // Frame was displayed.
    DROPPED,    // Frame was dropped.
    RESET,      // Scheduler was reset before frame was scheduled for display.
  };
  typedef base::Callback<void(const scoped_refptr<VideoFrame>&, Reason)> DoneCB;

  // Schedule |frame| to be displayed at |wall_ticks|, firing |done_cb| when
  // the scheduler has finished with the frame.
  virtual void ScheduleVideoFrame(const scoped_refptr<VideoFrame>& frame,
                                  base::TimeTicks wall_ticks,
                                  const DoneCB& done_cb) = 0;

  // Causes the scheduler to release all previously scheduled frames. Frames
  // will be returned as RESET.
  virtual void Reset() = 0;
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_FRAME_SCHEDULER_H_
