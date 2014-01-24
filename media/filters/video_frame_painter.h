// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_FRAME_PAINTER_H_
#define MEDIA_FILTERS_VIDEO_FRAME_PAINTER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/base/media_export.h"
#include "ui/gfx/size.h"

namespace media {

class VideoFrame;

// VideoFramePainter handles incoming frames by managing painting-related
// information and rate-limiting paint invalidations in a thread-safe manner.
//
// Typical usage is to deliver the output of VideoRendererImpl to
// UpdateCurrentFrame() so that VideoFramePainter can take care of tracking
// dropped frames and firing callbacks as needed.
//
// All APIs are callable from any thread.
class MEDIA_EXPORT VideoFramePainter {
 public:
  // |invalidate_cb| is run whenever the current frame has been updated and
  // the previous invalidation is no longer pending.
  //
  // |natural_size_changed_cb| is run with the new natural size of the video
  // frame whenever a change in natural size is detected.
  VideoFramePainter(
      const base::Closure& invalidate_cb,
      const base::Callback<void(gfx::Size)>& natural_size_changed_cb);
  ~VideoFramePainter();

  // Updates the current frame and runs |invalidate_cb| if there isn't an
  // outstanding invalidation.
  //
  // Clients are expected to run DidFinishInvalidating() to inform the painter
  // that invalidation has completed.
  void UpdateCurrentFrame(const scoped_refptr<VideoFrame>& frame);

  // Retrieves the last frame set via SetCurrentFrame().
  //
  // Clients must specify whether the frame will be painted to allow proper
  // dropped frame count.
  scoped_refptr<VideoFrame> GetCurrentFrame(bool frame_being_painted);

  // Informs the painter that the client has finished invalidating.
  void DidFinishInvalidating();

  // Returns the number of frames dropped before the painter was notified of
  // painting via DidPaintCurrentFrame().
  uint32 GetFramesDroppedBeforePaint();

  void SetFramesDroppedBeforePaintForTesting(uint32 dropped_frames);

 private:
  base::Closure invalidate_cb_;
  base::Callback<void(gfx::Size)> natural_size_changed_cb_;

  base::Lock lock_;
  scoped_refptr<VideoFrame> current_frame_;
  bool invalidation_finished_;
  bool current_frame_painted_;
  uint32 frames_dropped_before_paint_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoFramePainter);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_FRAME_PAINTER_H_
