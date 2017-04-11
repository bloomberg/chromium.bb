// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/in_process_video_capture_provider.h"

#include "content/browser/renderer_host/media/in_process_buildable_video_capture_device.h"

namespace content {

InProcessVideoCaptureProvider::InProcessVideoCaptureProvider(
    std::unique_ptr<media::VideoCaptureSystem> video_capture_system,
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner)
    : video_capture_system_(std::move(video_capture_system)),
      device_task_runner_(std::move(device_task_runner)) {}

InProcessVideoCaptureProvider::~InProcessVideoCaptureProvider() = default;

void InProcessVideoCaptureProvider::GetDeviceInfosAsync(
    const base::Callback<void(
        const std::vector<media::VideoCaptureDeviceInfo>&)>& result_callback) {
  // Using Unretained() is safe because |this| owns |video_capture_system_| and
  // |result_callback| has ownership of |this|.
  device_task_runner_->PostTask(
      FROM_HERE, base::Bind(&media::VideoCaptureSystem::GetDeviceInfosAsync,
                            base::Unretained(video_capture_system_.get()),
                            result_callback));
}

std::unique_ptr<BuildableVideoCaptureDevice>
InProcessVideoCaptureProvider::CreateBuildableDevice(
    const std::string& device_id,
    MediaStreamType stream_type) {
  return base::MakeUnique<InProcessBuildableVideoCaptureDevice>(
      device_task_runner_, video_capture_system_.get());
}

}  // namespace content
