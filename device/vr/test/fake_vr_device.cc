// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/fake_vr_device.h"

namespace device {

FakeVRDevice::FakeVRDevice() {
  display_info_ = mojom::VRDisplayInfo::New();

  InitBasicDevice();
}

FakeVRDevice::~FakeVRDevice() {}

void FakeVRDevice::InitBasicDevice() {
  display_info_->index = id();
  display_info_->displayName = "FakeVRDevice";

  display_info_->capabilities = mojom::VRDisplayCapabilities::New();
  display_info_->capabilities->hasPosition = false;
  display_info_->capabilities->hasExternalDisplay = false;
  display_info_->capabilities->canPresent = false;

  display_info_->leftEye = InitEye(45, -0.03f, 1024);
  display_info_->rightEye = InitEye(45, 0.03f, 1024);
}

mojom::VREyeParametersPtr FakeVRDevice::InitEye(float fov,
                                                float offset,
                                                uint32_t size) {
  mojom::VREyeParametersPtr eye = mojom::VREyeParameters::New();

  eye->fieldOfView = mojom::VRFieldOfView::New();
  eye->fieldOfView->upDegrees = fov;
  eye->fieldOfView->downDegrees = fov;
  eye->fieldOfView->leftDegrees = fov;
  eye->fieldOfView->rightDegrees = fov;

  eye->offset.resize(3);
  eye->offset[0] = offset;
  eye->offset[1] = 0.0f;
  eye->offset[2] = 0.0f;

  eye->renderWidth = size;
  eye->renderHeight = size;

  return eye;
}

void FakeVRDevice::SetVRDevice(const mojom::VRDisplayInfoPtr& display_info) {
  display_info_ = display_info.Clone();
}

void FakeVRDevice::CreateVRDisplayInfo(
    const base::Callback<void(mojom::VRDisplayInfoPtr)>& on_created) {
  mojom::VRDisplayInfoPtr display = display_info_.Clone();
  on_created.Run(std::move(display));
}

void FakeVRDevice::RequestPresent(mojom::VRSubmitFrameClientPtr submit_client,
                                  mojom::VRPresentationProviderRequest request,
                                  const base::Callback<void(bool)>& callback) {
  callback.Run(true);
}

void FakeVRDevice::SetSecureOrigin(bool secure_origin) {}

void FakeVRDevice::ExitPresent() {
  OnExitPresent();
}

void FakeVRDevice::GetNextMagicWindowPose(
    VRDisplayImpl* display,
    mojom::VRDisplay::GetNextMagicWindowPoseCallback callback) {
  std::move(callback).Run(nullptr);
}

}  // namespace device
