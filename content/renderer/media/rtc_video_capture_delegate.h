// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_VIDEO_CAPTURE_DELEGATE_H_
#define CONTENT_RENDERER_MEDIA_RTC_VIDEO_CAPTURE_DELEGATE_H_

#include "base/callback.h"
#include "base/message_loop_proxy.h"
#include "content/common/media/video_capture.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "media/video/capture/video_capture.h"

// Implements a simple reference counted video capturer that guarantees that
// methods in RtcVideoCaptureDelegateEventHandler is only called from when
// StartCapture have been called until after StopCapture have been called.
// It uses VideoCaptureImplManager to start / stop and receive I420 frames
// from Chrome's video capture implementation.
class RtcVideoCaptureDelegate
    : public base::RefCountedThreadSafe<RtcVideoCaptureDelegate>,
      public media::VideoCapture::EventHandler {
 public:
  typedef base::Callback<void(const media::VideoCapture::VideoFrameBuffer& buf)>
      FrameCapturedCallback;

  RtcVideoCaptureDelegate(const media::VideoCaptureSessionId id,
                          VideoCaptureImplManager* vc_manager);

  void StartCapture(const media::VideoCaptureCapability& capability,
                    const FrameCapturedCallback& callback);
  void StopCapture();

  // media::VideoCapture::EventHandler implementation.
  // These functions are called from a thread owned by |vc_manager_|.
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

 private:
  friend class base::RefCountedThreadSafe<RtcVideoCaptureDelegate>;

  virtual ~RtcVideoCaptureDelegate();

  void OnBufferReadyOnCaptureThread(
      media::VideoCapture* capture,
      scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf);

  // The id identifies which video capture device is used for this video
  // capture session.
  media::VideoCaptureSessionId session_id_;
  // The video capture manager handles open/close of video capture devices.
  scoped_refptr<VideoCaptureImplManager> vc_manager_;
  media::VideoCapture* capture_engine_;
  // |captured_callback_| is provided to this class in StartCapture and must be
  // valid until StopCapture is called.
  FrameCapturedCallback captured_callback_;
  // MessageLoop of the caller of StartCapture.
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
};

#endif  // CONTENT_RENDERER_MEDIA_RTC_VIDEO_CAPTURE_DELEGATE_H_
