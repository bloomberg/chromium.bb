// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_BUILDABLE_VIDEO_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_BUILDABLE_VIDEO_CAPTURE_DEVICE_H_

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_info.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"

namespace content {

class LaunchedVideoCaptureDevice;

// Asynchronously launches video capture devices. After a call to
// LaunchDeviceAsync() it is illegal to call LaunchDeviceAsync() again until
// |callbacks| has been notified about the outcome of the asynchronous launch.
class CONTENT_EXPORT VideoCaptureDeviceLauncher {
 public:
  class CONTENT_EXPORT Callbacks {
   public:
    virtual ~Callbacks() {}
    virtual void OnDeviceLaunched(
        std::unique_ptr<LaunchedVideoCaptureDevice> device) = 0;
    virtual void OnDeviceLaunchFailed() = 0;
    virtual void OnDeviceLaunchAborted() = 0;
  };

  virtual ~VideoCaptureDeviceLauncher() {}

  // The passed-in |done_cb| must guarantee that the context relevant
  // during the asynchronous processing stays alive.
  virtual void LaunchDeviceAsync(
      const std::string& device_id,
      MediaStreamType stream_type,
      const media::VideoCaptureParams& params,
      base::WeakPtr<media::VideoFrameReceiver> receiver,
      Callbacks* callbacks,
      base::OnceClosure done_cb) = 0;

  virtual void AbortLaunch() = 0;
};

class LaunchedVideoCaptureDevice
    : public media::VideoFrameConsumerFeedbackObserver {
 public:
  // Device operation methods.
  virtual void GetPhotoCapabilities(
      media::VideoCaptureDevice::GetPhotoCapabilitiesCallback callback)
      const = 0;
  virtual void SetPhotoOptions(
      media::mojom::PhotoSettingsPtr settings,
      media::VideoCaptureDevice::SetPhotoOptionsCallback callback) = 0;
  virtual void TakePhoto(
      media::VideoCaptureDevice::TakePhotoCallback callback) = 0;
  virtual void MaybeSuspendDevice() = 0;
  virtual void ResumeDevice() = 0;
  virtual void RequestRefreshFrame() = 0;

  // Methods for specific types of devices.
  virtual void SetDesktopCaptureWindowIdAsync(gfx::NativeViewId window_id,
                                              base::OnceClosure done_cb) = 0;
};

// Note: GetDeviceInfosAsync is only relevant for devices with
// MediaStreamType == MEDIA_DEVICE_VIDEO_CAPTURE, i.e. camera devices.
class CONTENT_EXPORT VideoCaptureProvider {
 public:
  using GetDeviceInfosCallback =
      base::Callback<void(const std::vector<media::VideoCaptureDeviceInfo>&)>;

  virtual ~VideoCaptureProvider() {}

  virtual void Uninitialize() = 0;

  // The passed-in |result_callback| must guarantee that the called
  // instance stays alive until |result_callback| is invoked.
  virtual void GetDeviceInfosAsync(
      const GetDeviceInfosCallback& result_callback) = 0;

  virtual std::unique_ptr<VideoCaptureDeviceLauncher>
  CreateDeviceLauncher() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_BUILDABLE_VIDEO_CAPTURE_DEVICE_H_
