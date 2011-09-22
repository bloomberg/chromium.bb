// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MODULE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MODULE_IMPL_H_

#include "base/compiler_specific.h"
#include "base/threading/thread.h"
#include "media/video/capture/video_capture.h"
#include "third_party/webrtc/common_types.h"
#include "third_party/webrtc/modules/video_capture/main/interface/video_capture_defines.h"
#include "third_party/webrtc/modules/video_capture/main/source/video_capture_impl.h"

class VideoCaptureImplManager;

// An implementation of webrtc::VideoCaptureModule takes raw frames from video
// capture engine and passed them to webrtc VideoEngine.
class VideoCaptureModuleImpl
    : public webrtc::videocapturemodule::VideoCaptureImpl,
      public media::VideoCapture::EventHandler {
 public:
  VideoCaptureModuleImpl(const media::VideoCaptureSessionId id,
                         VideoCaptureImplManager* vc_manager);
  // Implement reference counting. This make it possible to reference the
  // object from both Chromium and WebRtc.
  virtual int32_t AddRef() OVERRIDE;
  virtual int32_t Release() OVERRIDE;

  // Override webrtc::videocapturemodule::VideoCaptureImpl implementation.
  virtual WebRtc_Word32 StartCapture(
      const webrtc::VideoCaptureCapability& capability) OVERRIDE;
  virtual WebRtc_Word32 StopCapture() OVERRIDE;
  virtual bool CaptureStarted() OVERRIDE;
  virtual WebRtc_Word32 CaptureSettings(
      webrtc::VideoCaptureCapability& settings) OVERRIDE;

  // media::VideoCapture::EventHandler implementation.
  virtual void OnStarted(media::VideoCapture* capture);
  virtual void OnStopped(media::VideoCapture* capture);
  virtual void OnPaused(media::VideoCapture* capture);
  virtual void OnError(media::VideoCapture* capture, int error_code);
  virtual void OnBufferReady(
      media::VideoCapture* capture,
      scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf);
  virtual void OnDeviceInfoReceived(
      media::VideoCapture* capture,
      const media::VideoCaptureParams& device_info);

 private:
  virtual ~VideoCaptureModuleImpl();
  void Init();

  void StartCaptureOnCaptureThread(
      const webrtc::VideoCaptureCapability& capability);
  void StopCaptureOnCaptureThread();
  void StartCaptureInternal(const webrtc::VideoCaptureCapability& capability);

  void OnStoppedOnCaptureThread(media::VideoCapture* capture);
  void OnBufferReadyOnCaptureThread(
      media::VideoCapture* capture,
      scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf);

  // The id identifies which video capture device is used for this video
  // capture session.
  media::VideoCaptureSessionId session_id_;
  base::Thread thread_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  // The video capture manager handles open/close of video capture devices.
  scoped_refptr<VideoCaptureImplManager> vc_manager_;
  media::VideoCapture::State state_;
  bool got_first_frame_;
  WebRtc_UWord32 width_;
  WebRtc_UWord32 height_;
  WebRtc_Word32 frame_rate_;
  webrtc::RawVideoType video_type_;
  webrtc::VideoCaptureCapability frameInfo_;
  base::Time start_time_;
  // The video capture module generating raw frame data.
  media::VideoCapture* capture_engine_;
  bool pending_start_;
  webrtc::VideoCaptureCapability pending_cap_;
  int ref_count_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureModuleImpl);
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(VideoCaptureModuleImpl);

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MODULE_IMPL_H_
