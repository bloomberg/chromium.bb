// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/in_process_video_capture_provider.h"

#include "content/browser/renderer_host/media/in_process_video_capture_device_launcher.h"

namespace content {

InProcessVideoCaptureProvider::InProcessVideoCaptureProvider(
    std::unique_ptr<media::VideoCaptureSystem> video_capture_system,
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner)
    : video_capture_system_(std::move(video_capture_system)),
      device_task_runner_(std::move(device_task_runner)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

InProcessVideoCaptureProvider::~InProcessVideoCaptureProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<VideoCaptureProvider>
InProcessVideoCaptureProvider::CreateInstanceForNonDeviceCapture(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner) {
  return base::MakeUnique<InProcessVideoCaptureProvider>(
      nullptr, std::move(device_task_runner));
}

// static
std::unique_ptr<VideoCaptureProvider>
InProcessVideoCaptureProvider::CreateInstance(
    std::unique_ptr<media::VideoCaptureSystem> video_capture_system,
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner) {
  return base::MakeUnique<InProcessVideoCaptureProvider>(
      std::move(video_capture_system), std::move(device_task_runner));
}

void InProcessVideoCaptureProvider::GetDeviceInfosAsync(
    GetDeviceInfosCallback result_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!video_capture_system_) {
    std::vector<media::VideoCaptureDeviceInfo> empty_result;
    base::ResetAndReturn(&result_callback).Run(empty_result);
    return;
  }
  // Using Unretained() is safe because |this| owns
  // |video_capture_system_| and |result_callback| has ownership of
  // |this|.
  device_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&media::VideoCaptureSystem::GetDeviceInfosAsync,
                                base::Unretained(video_capture_system_.get()),
                                std::move(result_callback)));
}

std::unique_ptr<VideoCaptureDeviceLauncher>
InProcessVideoCaptureProvider::CreateDeviceLauncher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::MakeUnique<InProcessVideoCaptureDeviceLauncher>(
      device_task_runner_, video_capture_system_.get());
}

}  // namespace content
