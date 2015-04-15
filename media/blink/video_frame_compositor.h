// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_VIDEO_FRAME_COMPOSITOR_H_
#define MEDIA_BLINK_VIDEO_FRAME_COMPOSITOR_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "cc/layers/video_frame_provider.h"
#include "media/base/media_export.h"
#include "media/base/video_renderer_sink.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class VideoFrame;

// VideoFrameCompositor acts as a bridge between the media and cc layers for
// rendering video frames. I.e. a media::VideoRenderer will talk to this class
// from the media side, while a cc::VideoFrameProvider::Client will talk to it
// from the cc side.
//
// This class is responsible for requesting new frames from a video renderer in
// response to requests from the VFP::Client. Since the VFP::Client may stop
// issuing requests in response to visibility changes it is also responsible for
// ensuring the "freshness" of the current frame for programmatic frame
// requests; e.g., Canvas.drawImage() requests
//
// This class is also responsible for detecting frames dropped by the compositor
// after rendering and signaling that information to a RenderCallback. It
// detects frames not dropped by verifying each GetCurrentFrame() is followed
// by a PutCurrentFrame() before the next UpdateCurrentFrame() call.
//
// VideoRenderSink::RenderCallback implementations must call Start() and Stop()
// once new frames are expected or are no longer expected to be ready; this data
// is relayed to the compositor to avoid extraneous callbacks.
//
// VideoFrameCompositor must live on the same thread as the compositor, though
// it may be constructed on any thread.
class MEDIA_EXPORT VideoFrameCompositor
    : public VideoRendererSink,
      NON_EXPORTED_BASE(public cc::VideoFrameProvider) {
 public:
  // |compositor_task_runner| is the task runner on which this class will live,
  // though it may be constructed on any thread.
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
  // TODO(dalecurtis): Investigate the inconsistency between the callbacks with
  // respect to why we don't call |natural_size_changed_cb| on the first frame.
  // I suspect it was for historical reasons that no longer make sense.
  VideoFrameCompositor(
      const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
      const base::Callback<void(gfx::Size)>& natural_size_changed_cb,
      const base::Callback<void(bool)>& opacity_changed_cb);

  // Destruction must happen on the compositor thread; Stop() must have been
  // called before destruction starts.
  ~VideoFrameCompositor() override;

  // Returns |current_frame_| if it was refreshed recently; otherwise, if
  // |callback_| is available, requests a new frame and returns that one.
  //
  // This is required for programmatic frame requests where the compositor may
  // have stopped issuing UpdateCurrentFrame() callbacks in response to
  // visibility changes in the output layer.
  scoped_refptr<VideoFrame> GetCurrentFrameAndUpdateIfStale();

  // cc::VideoFrameProvider implementation. These methods must be called on the
  // |compositor_task_runner_|.
  void SetVideoFrameProviderClient(
      cc::VideoFrameProvider::Client* client) override;
  bool UpdateCurrentFrame(base::TimeTicks deadline_min,
                          base::TimeTicks deadline_max) override;
  scoped_refptr<VideoFrame> GetCurrentFrame() override;
  void PutCurrentFrame() override;

  // VideoRendererSink implementation. These methods must be called from the
  // same thread (typically the media thread).
  void Start(RenderCallback* callback) override;
  void Stop() override;
  void PaintFrameUsingOldRenderingPath(
      const scoped_refptr<VideoFrame>& frame) override;

 private:
  // Called on the compositor thread to start or stop rendering if rendering was
  // previously started or stopped before we had a |callback_|.
  void OnRendererStateUpdate();

  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  // These callbacks are executed on the compositor thread.
  base::Callback<void(gfx::Size)> natural_size_changed_cb_;
  base::Callback<void(bool)> opacity_changed_cb_;

  // These values are only set and read on the compositor thread.
  cc::VideoFrameProvider::Client* client_;
  scoped_refptr<VideoFrame> current_frame_;

  // These values are updated and read from the media and compositor threads.
  base::Lock lock_;
  bool rendering_;
  VideoRendererSink::RenderCallback* callback_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameCompositor);
};

}  // namespace media

#endif  // MEDIA_BLINK_VIDEO_FRAME_COMPOSITOR_H_
