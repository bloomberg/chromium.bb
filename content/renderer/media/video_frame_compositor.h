// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_FRAME_COMPOSITOR_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_FRAME_COMPOSITOR_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "ui/gfx/size.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class VideoFrameProvider;
}

namespace media {
class VideoFrame;
}

namespace content {

// VideoFrameCompositor handles incoming frames by notifying the compositor in a
// thread-safe manner.
//
// Typical usage is to deliver the output of VideoRendererImpl to
// UpdateCurrentFrame() so that VideoFrameCompositor can take care of tracking
// dropped frames and firing callbacks as needed.
//
// All APIs are callable from any thread.
class CONTENT_EXPORT VideoFrameCompositor {
 public:
  // |compositor_task_runner| is the task runner of the compositor.
  //
  // |natural_size_changed_cb| is run with the new natural size of the video
  // frame whenever a change in natural size is detected. It is not called the
  // first time UpdateCurrentFrame() is called. Run on the same thread as the
  // caller of UpdateCurrentFrame().
  //
  // |opacity_changed_cb| is run when a change in opacity is detected. It *is*
  // called the first time UpdateCurrentFrame() is called. Run on the same
  // thread as the caller of UpdateCurrentFrame().
  //
  // TODO(scherkus): Investigate the inconsistency between the callbacks with
  // respect to why we don't call |natural_size_changed_cb| on the first frame.
  // I suspect it was for historical reasons that no longer make sense.
  VideoFrameCompositor(
      const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
      const base::Callback<void(gfx::Size)>& natural_size_changed_cb,
      const base::Callback<void(bool)>& opacity_changed_cb);
  ~VideoFrameCompositor();

  cc::VideoFrameProvider* GetVideoFrameProvider();

  // Updates the current frame and notifies the compositor.
  void UpdateCurrentFrame(const scoped_refptr<media::VideoFrame>& frame);

  // Retrieves the last frame set via UpdateCurrentFrame() for non-compositing
  // purposes (e.g., painting to a canvas).
  //
  // Note that the compositor retrieves frames via the cc::VideoFrameProvider
  // interface instead of using this method.
  scoped_refptr<media::VideoFrame> GetCurrentFrame();

  // Returns the number of frames dropped before the compositor was notified
  // of a new frame.
  uint32 GetFramesDroppedBeforeCompositorWasNotified();

  void SetFramesDroppedBeforeCompositorWasNotifiedForTesting(
      uint32 dropped_frames);

 private:
  class Internal;
  Internal* internal_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameCompositor);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_FRAME_COMPOSITOR_H_
