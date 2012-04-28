// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_VIDEO_CAPTURE_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_VIDEO_CAPTURE_IMPL_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/video/capture/video_capture.h"
#include "media/video/capture/video_capture_types.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

namespace media {
class VideoCaptureHandlerProxy;
}

namespace content {

class PepperPluginDelegateImpl;

class PepperPlatformVideoCaptureImpl
    : public webkit::ppapi::PluginDelegate::PlatformVideoCapture,
      public media::VideoCapture::EventHandler {
 public:
  PepperPlatformVideoCaptureImpl(
      const base::WeakPtr<PepperPluginDelegateImpl>& plugin_delegate,
      const std::string& device_id,
      webkit::ppapi::PluginDelegate::PlatformVideoCaptureEventHandler* handler);

  // webkit::ppapi::PluginDelegate::PlatformVideoCapture implementation.
  virtual void StartCapture(
      media::VideoCapture::EventHandler* handler,
      const media::VideoCaptureCapability& capability) OVERRIDE;
  virtual void StopCapture(media::VideoCapture::EventHandler* handler) OVERRIDE;
  virtual void FeedBuffer(scoped_refptr<VideoFrameBuffer> buffer) OVERRIDE;
  virtual bool CaptureStarted() OVERRIDE;
  virtual int CaptureWidth() OVERRIDE;
  virtual int CaptureHeight() OVERRIDE;
  virtual int CaptureFrameRate() OVERRIDE;
  virtual void DetachEventHandler() OVERRIDE;

  // media::VideoCapture::EventHandler implementation
  virtual void OnStarted(VideoCapture* capture) OVERRIDE;
  virtual void OnStopped(VideoCapture* capture) OVERRIDE;
  virtual void OnPaused(VideoCapture* capture) OVERRIDE;
  virtual void OnError(VideoCapture* capture, int error_code) OVERRIDE;
  virtual void OnRemoved(VideoCapture* capture) OVERRIDE;
  virtual void OnBufferReady(VideoCapture* capture,
                             scoped_refptr<VideoFrameBuffer> buffer) OVERRIDE;
  virtual void OnDeviceInfoReceived(
      VideoCapture* capture,
      const media::VideoCaptureParams& device_info) OVERRIDE;

 protected:
  virtual ~PepperPlatformVideoCaptureImpl();

 private:
  void Initialize();

  void OnDeviceOpened(int request_id,
                      bool succeeded,
                      const std::string& label);

  base::WeakPtr<PepperPluginDelegateImpl> plugin_delegate_;

  std::string device_id_;
  std::string label_;
  int session_id_;

  scoped_ptr<media::VideoCaptureHandlerProxy> handler_proxy_;

  webkit::ppapi::PluginDelegate::PlatformVideoCaptureEventHandler* handler_;

  media::VideoCapture* video_capture_;

  // StartCapture() must be balanced by StopCapture(), otherwise this object
  // will leak.
  bool unbalanced_start_;

  DISALLOW_COPY_AND_ASSIGN(PepperPlatformVideoCaptureImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_VIDEO_CAPTURE_IMPL_H_
