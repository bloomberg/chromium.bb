// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_LAUNCHED_VIDEO_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_LAUNCHED_VIDEO_CAPTURE_DEVICE_H_

#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "services/video_capture/public/interfaces/device.mojom.h"

namespace content {

// Implementation of LaunchedVideoCaptureDevice that uses the "video_capture"
// service.
class ServiceLaunchedVideoCaptureDevice : public LaunchedVideoCaptureDevice {
 public:
  explicit ServiceLaunchedVideoCaptureDevice(
      video_capture::mojom::DevicePtr device);
  ~ServiceLaunchedVideoCaptureDevice() override;

  // LaunchedVideoCaptureDevice implementation.
  void GetPhotoCapabilities(
      media::VideoCaptureDevice::GetPhotoCapabilitiesCallback callback)
      const override;
  void SetPhotoOptions(
      media::mojom::PhotoSettingsPtr settings,
      media::VideoCaptureDevice::SetPhotoOptionsCallback callback) override;
  void TakePhoto(
      media::VideoCaptureDevice::TakePhotoCallback callback) override;
  void MaybeSuspendDevice() override;
  void ResumeDevice() override;
  void RequestRefreshFrame() override;

  void SetDesktopCaptureWindowIdAsync(gfx::NativeViewId window_id,
                                      base::OnceClosure done_cb) override;

  void OnUtilizationReport(int frame_feedback_id, double utilization) override;

 private:
  void OnLostConnectionToDevice();

  video_capture::mojom::DevicePtr device_;
  base::SequenceChecker sequence_checker_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_LAUNCHED_VIDEO_CAPTURE_DEVICE_H_
