// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_BUILDABLE_VIDEO_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_BUILDABLE_VIDEO_CAPTURE_DEVICE_H_

#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/public/common/media_stream_request.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video/video_capture_system.h"

namespace content {

// Implementation of BuildableVideoCaptureDevice that creates capture devices
// in the same process as it is being operated on, which must be the Browser
// process. The devices are operated on the given |device_task_runner|.
// Instances of this class must be operated from the Browser process IO thread.
class InProcessBuildableVideoCaptureDevice
    : public BuildableVideoCaptureDevice {
 public:
  InProcessBuildableVideoCaptureDevice(
      scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
      media::VideoCaptureSystem* video_capture_system);
  ~InProcessBuildableVideoCaptureDevice() override;

  // BuildableVideoCaptureDevice implementation:
  void CreateAndStartDeviceAsync(VideoCaptureController* controller,
                                 const media::VideoCaptureParams& params,
                                 Callbacks* callbacks,
                                 base::OnceClosure done_cb) override;
  void ReleaseDeviceAsync(VideoCaptureController* controller,
                          base::OnceClosure done_cb) override;
  bool IsDeviceAlive() const override;
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

 private:
  using ReceiveDeviceCallback =
      base::Callback<void(std::unique_ptr<media::VideoCaptureDevice> device)>;

  enum class State {
    NO_DEVICE,
    DEVICE_START_IN_PROGRESS,
    DEVICE_START_ABORTING,
    DEVICE_STARTED
  };

  std::unique_ptr<media::VideoCaptureDeviceClient> CreateDeviceClient(
      int buffer_pool_max_buffer_count,
      base::WeakPtr<media::VideoFrameReceiver> receiver);

  void OnDeviceStarted(VideoCaptureController* controller,
                       Callbacks* callbacks,
                       base::OnceClosure done_cb,
                       std::unique_ptr<media::VideoCaptureDevice> device);

  void DoStartDeviceCaptureOnDeviceThread(
      const std::string& device_id,
      const media::VideoCaptureParams& params,
      std::unique_ptr<media::VideoCaptureDeviceClient> client,
      ReceiveDeviceCallback result_callback);

  void DoStartTabCaptureOnDeviceThread(
      const std::string& device_id,
      const media::VideoCaptureParams& params,
      std::unique_ptr<media::VideoCaptureDeviceClient> client,
      ReceiveDeviceCallback result_callback);

  void DoStartDesktopCaptureOnDeviceThread(
      const std::string& device_id,
      const media::VideoCaptureParams& params,
      std::unique_ptr<media::VideoCaptureDeviceClient> client,
      ReceiveDeviceCallback result_callback);

  void SetDesktopCaptureWindowIdOnDeviceThread(
      media::VideoCaptureDevice* device,
      gfx::NativeViewId window_id,
      base::OnceClosure done_cb);

  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;
  media::VideoCaptureSystem* const video_capture_system_;
  std::unique_ptr<media::VideoCaptureDevice> device_;
  State state_ = State::NO_DEVICE;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_BUILDABLE_VIDEO_CAPTURE_DEVICE_H_
