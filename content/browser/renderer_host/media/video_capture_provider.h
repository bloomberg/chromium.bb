// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_BUILDABLE_VIDEO_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_BUILDABLE_VIDEO_CAPTURE_DEVICE_H_

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "content/public/common/media_stream_request.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_info.h"
#include "media/capture/video_capture_types.h"

namespace content {

class VideoCaptureController;

// Abstraction for a video capture device that must be "built" before it can be
// operated and must be "stopped", before it can be released. Typical operation
// is that a newly created instance initially reports IsDeviceAlive() == false.
// Clients call CreateAndStartDeviceAsync(), which kicks off the asynchronous
// building of the device. The outcome of the device building is reported to an
// instance of Callbacks. Once the device has been built successfully, the
// "Device operation methods", are allowed to be called. ReleaseDeviceAsync()
// must be called in order to release the device if it has before been built
// successfully. After calling ReleaseDeviceAsync(), it is legal to call
// CreateAndStartDeviceAsync() to rebuild and start the device again.
class CONTENT_EXPORT BuildableVideoCaptureDevice {
 public:
  class CONTENT_EXPORT Callbacks {
   public:
    virtual ~Callbacks() {}
    virtual void OnDeviceStarted(VideoCaptureController* controller) = 0;
    virtual void OnDeviceStartFailed(VideoCaptureController* controller) = 0;
    virtual void OnDeviceStartAborted() = 0;
  };

  virtual ~BuildableVideoCaptureDevice() {}

  // Device management methods.
  virtual void CreateAndStartDeviceAsync(
      VideoCaptureController* controller,
      const media::VideoCaptureParams& params,
      Callbacks* callbacks,
      base::OnceClosure done_cb) = 0;
  virtual void ReleaseDeviceAsync(VideoCaptureController* controller,
                                  base::OnceClosure done_cb) = 0;
  virtual bool IsDeviceAlive() const = 0;

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

class CONTENT_EXPORT VideoCaptureProvider {
 public:
  virtual ~VideoCaptureProvider() {}

  // The passed-in |result_callback| must guarantee that the called
  // instance stays alive until |result_callback| is invoked.
  virtual void GetDeviceInfosAsync(
      const base::Callback<
          void(const std::vector<media::VideoCaptureDeviceInfo>&)>&
          result_callback) = 0;

  virtual std::unique_ptr<BuildableVideoCaptureDevice> CreateBuildableDevice(
      const std::string& device_id,
      MediaStreamType stream_type) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_BUILDABLE_VIDEO_CAPTURE_DEVICE_H_
