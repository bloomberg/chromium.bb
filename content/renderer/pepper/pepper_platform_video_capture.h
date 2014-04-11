// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_VIDEO_CAPTURE_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_VIDEO_CAPTURE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/video/capture/video_capture.h"
#include "media/video/capture/video_capture_types.h"

class GURL;

namespace media {
class VideoCaptureHandlerProxy;
}

namespace content {
class PepperMediaDeviceManager;
class PepperVideoCaptureHost;
class RenderViewImpl;
class VideoCaptureHandle;

class PepperPlatformVideoCapture
    : public media::VideoCapture,
      public base::RefCounted<PepperPlatformVideoCapture>,
      public media::VideoCapture::EventHandler {
 public:
  PepperPlatformVideoCapture(const base::WeakPtr<RenderViewImpl>& render_view,
                             const std::string& device_id,
                             const GURL& document_url,
                             PepperVideoCaptureHost* handler);

  // Detaches the event handler and stops sending notifications to it.
  void DetachEventHandler();

  // media::VideoCapture implementation.
  virtual void StartCapture(media::VideoCapture::EventHandler* handler,
                            const media::VideoCaptureParams& params) OVERRIDE;
  virtual void StopCapture(media::VideoCapture::EventHandler* handler) OVERRIDE;
  virtual bool CaptureStarted() OVERRIDE;
  virtual int CaptureFrameRate() OVERRIDE;
  virtual void GetDeviceSupportedFormats(const DeviceFormatsCallback& callback)
      OVERRIDE;
  virtual void GetDeviceFormatsInUse(const DeviceFormatsInUseCallback& callback)
      OVERRIDE;

  // media::VideoCapture::EventHandler implementation
  virtual void OnStarted(VideoCapture* capture) OVERRIDE;
  virtual void OnStopped(VideoCapture* capture) OVERRIDE;
  virtual void OnPaused(VideoCapture* capture) OVERRIDE;
  virtual void OnError(VideoCapture* capture, int error_code) OVERRIDE;
  virtual void OnRemoved(VideoCapture* capture) OVERRIDE;
  virtual void OnFrameReady(VideoCapture* capture,
                            const scoped_refptr<media::VideoFrame>& frame)
      OVERRIDE;

 protected:
  friend class base::RefCounted<PepperPlatformVideoCapture>;
  virtual ~PepperPlatformVideoCapture();

 private:
  void Initialize();

  void OnDeviceOpened(int request_id, bool succeeded, const std::string& label);

  PepperMediaDeviceManager* GetMediaDeviceManager();

  base::WeakPtr<RenderViewImpl> render_view_;

  std::string device_id_;
  std::string label_;
  int session_id_;

  scoped_ptr<media::VideoCaptureHandlerProxy> handler_proxy_;

  PepperVideoCaptureHost* handler_;

  scoped_ptr<VideoCaptureHandle> video_capture_;

  // StartCapture() must be balanced by StopCapture(), otherwise this object
  // will leak.
  bool unbalanced_start_;

  // Whether we have a pending request to open a device. We have to make sure
  // there isn't any pending request before this object goes away.
  bool pending_open_device_;
  int pending_open_device_id_;

  DISALLOW_COPY_AND_ASSIGN(PepperPlatformVideoCapture);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_VIDEO_CAPTURE_H_
