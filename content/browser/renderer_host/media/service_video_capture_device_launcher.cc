// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"

namespace content {

ServiceVideoCaptureDeviceLauncher::ServiceVideoCaptureDeviceLauncher(
    video_capture::mojom::DeviceFactoryPtr* device_factory)
    : device_factory_(device_factory) {}

ServiceVideoCaptureDeviceLauncher::~ServiceVideoCaptureDeviceLauncher() {}

void ServiceVideoCaptureDeviceLauncher::LaunchDeviceAsync(
    const std::string& device_id,
    MediaStreamType stream_type,
    const media::VideoCaptureParams& params,
    base::WeakPtr<media::VideoFrameReceiver> receiver,
    Callbacks* callbacks,
    base::OnceClosure done_cb) {
  NOTIMPLEMENTED();
}

void ServiceVideoCaptureDeviceLauncher::AbortLaunch() {
  NOTIMPLEMENTED();
}

}  // namespace content
