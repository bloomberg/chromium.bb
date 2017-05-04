// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_launched_video_capture_device.h"

namespace content {

ServiceLaunchedVideoCaptureDevice::ServiceLaunchedVideoCaptureDevice(
    video_capture::mojom::DevicePtr device)
    : device_(std::move(device)) {}

ServiceLaunchedVideoCaptureDevice::~ServiceLaunchedVideoCaptureDevice() {}

void ServiceLaunchedVideoCaptureDevice::GetPhotoCapabilities(
    media::VideoCaptureDevice::GetPhotoCapabilitiesCallback callback) const {
  NOTIMPLEMENTED();
}

void ServiceLaunchedVideoCaptureDevice::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    media::VideoCaptureDevice::SetPhotoOptionsCallback callback) {
  NOTIMPLEMENTED();
}

void ServiceLaunchedVideoCaptureDevice::TakePhoto(
    media::VideoCaptureDevice::TakePhotoCallback callback) {
  NOTIMPLEMENTED();
}

void ServiceLaunchedVideoCaptureDevice::MaybeSuspendDevice() {
  NOTIMPLEMENTED();
}

void ServiceLaunchedVideoCaptureDevice::ResumeDevice() {
  NOTIMPLEMENTED();
}

void ServiceLaunchedVideoCaptureDevice::RequestRefreshFrame() {
  NOTIMPLEMENTED();
}

void ServiceLaunchedVideoCaptureDevice::SetDesktopCaptureWindowIdAsync(
    gfx::NativeViewId window_id,
    base::OnceClosure done_cb) {
  NOTIMPLEMENTED();
}

void ServiceLaunchedVideoCaptureDevice::OnUtilizationReport(
    int frame_feedback_id,
    double utilization) {
  NOTIMPLEMENTED();
}

}  // namespace content
