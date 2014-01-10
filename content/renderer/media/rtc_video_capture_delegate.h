// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_VIDEO_CAPTURE_DELEGATE_H_
#define CONTENT_RENDERER_MEDIA_RTC_VIDEO_CAPTURE_DELEGATE_H_

#include "base/callback.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/common/media/video_capture.h"
#include "media/video/capture/video_capture.h"

namespace content {

class VideoCaptureHandle;

// Implements a simple reference counted video capturer that guarantees that
// methods in RtcVideoCaptureDelegateEventHandler is only called from when
// StartCapture have been called until after StopCapture have been called.
// It uses VideoCaptureImplManager to start / stop and receive I420 frames
// from Chrome's video capture implementation.
class RtcVideoCaptureDelegate
    : public base::RefCountedThreadSafe<RtcVideoCaptureDelegate>,
      public media::VideoCapture::EventHandler {
 public:
  enum CaptureState {
    CAPTURE_STOPPED,  // The capturer has been stopped or hasn't started yet.
    CAPTURE_RUNNING,  // The capturer has been started successfully and is now
                      // capturing.
    CAPTURE_FAILED,  // The capturer failed to start.
  };

  typedef base::Callback<void(const scoped_refptr<media::VideoFrame>&)>
      FrameCapturedCallback;
  typedef base::Callback<void(CaptureState)> StateChangeCallback;

  RtcVideoCaptureDelegate(const media::VideoCaptureSessionId id);

  void StartCapture(const media::VideoCaptureParams& params,
                    const FrameCapturedCallback& captured_callback,
                    const StateChangeCallback& state_callback);
  void StopCapture();

 protected:
  // media::VideoCapture::EventHandler implementation.
  // These functions are called on the IO thread (same as where
  // |capture_engine_| runs).
  virtual void OnStarted(media::VideoCapture* capture) OVERRIDE;
  virtual void OnStopped(media::VideoCapture* capture) OVERRIDE;
  virtual void OnPaused(media::VideoCapture* capture) OVERRIDE;
  virtual void OnError(media::VideoCapture* capture, int error_code) OVERRIDE;
  virtual void OnRemoved(media::VideoCapture* capture) OVERRIDE;
  virtual void OnFrameReady(
      media::VideoCapture* capture,
      const scoped_refptr<media::VideoFrame>& frame) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<RtcVideoCaptureDelegate>;

  virtual ~RtcVideoCaptureDelegate();

  void OnFrameReadyOnCaptureThread(
      media::VideoCapture* capture,
      const scoped_refptr<media::VideoFrame>& frame);
  void OnErrorOnCaptureThread(media::VideoCapture* capture);
  void OnRemovedOnCaptureThread(media::VideoCapture* capture);

  // The id identifies which video capture device is used for this video
  // capture session.
  media::VideoCaptureSessionId session_id_;
  scoped_ptr<VideoCaptureHandle> capture_engine_;

  // Accessed on the thread where StartCapture is called.
  bool got_first_frame_;
  bool error_occured_;

  // |captured_callback_| is provided to this class in StartCapture and must be
  // valid until StopCapture is called.
  FrameCapturedCallback captured_callback_;
  // |state_callback_| is provided to this class in StartCapture and must be
  // valid until StopCapture is called.
  StateChangeCallback state_callback_;
  // Message loop of the caller of StartCapture.
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_VIDEO_CAPTURE_DELEGATE_H_
