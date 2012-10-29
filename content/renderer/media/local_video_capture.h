// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_LOCAL_VIDEO_CAPTURE_H_
#define CONTENT_RENDERER_MEDIA_LOCAL_VIDEO_CAPTURE_H_

#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "media/video/capture/video_capture.h"
#include "webkit/media/video_frame_provider.h"

namespace base {
class MessageLoopProxy;
}

namespace media {
class VideoCaptureHandlerProxy;
}

namespace content {
class VideoCaptureImplManager;

// This class takes raw frames from video capture engine via VideoCaptureProxy
// and passes them to media player as a video frame provider.
// This class lives on main thread.
class CONTENT_EXPORT LocalVideoCapture
    : NON_EXPORTED_BASE(public webkit_media::VideoFrameProvider),
      public media::VideoCapture::EventHandler {
 public:
  LocalVideoCapture(
      media::VideoCaptureSessionId video_stream_id,
      VideoCaptureImplManager* vc_manager,
      const media::VideoCaptureCapability& capability,
      const base::Closure& error_cb,
      const RepaintCB& repaint_cb);

  // webkit_media::VideoFrameProvider implementation.
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual void Pause() OVERRIDE;

  // VideoCapture::EventHandler implementation.
  virtual void OnStarted(media::VideoCapture* capture) OVERRIDE;
  virtual void OnStopped(media::VideoCapture* capture) OVERRIDE;
  virtual void OnPaused(media::VideoCapture* capture) OVERRIDE;
  virtual void OnError(media::VideoCapture* capture, int error_code) OVERRIDE;
  virtual void OnRemoved(media::VideoCapture* capture) OVERRIDE;
  virtual void OnBufferReady(
      media::VideoCapture* capture,
      scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf) OVERRIDE;
  virtual void OnDeviceInfoReceived(
      media::VideoCapture* capture,
      const media::VideoCaptureParams& device_info) OVERRIDE;

 protected:
  virtual ~LocalVideoCapture();

 private:
  friend class LocalVideoCaptureTest;

  media::VideoCaptureSessionId video_stream_id_;
  scoped_refptr<VideoCaptureImplManager> vc_manager_;
  media::VideoCaptureCapability capability_;
  base::Closure error_cb_;
  RepaintCB repaint_cb_;
  media::VideoCapture* capture_engine_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  scoped_ptr<media::VideoCaptureHandlerProxy> handler_proxy_;
  VideoCaptureState state_;

  DISALLOW_COPY_AND_ASSIGN(LocalVideoCapture);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_LOCAL_VIDEO_CAPTURE_H_
