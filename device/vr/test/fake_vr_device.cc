// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/fake_vr_device.h"

namespace device {

FakeVRDevice::FakeVRDevice(VRDeviceProvider* provider) : VRDevice(provider) {
  device_ = mojom::VRDisplayInfo::New();
  pose_ = mojom::VRPose::New();

  InitBasicDevice();
}

FakeVRDevice::~FakeVRDevice() {}

void FakeVRDevice::InitBasicDevice() {
  device_->displayName = "FakeVRDevice";

  device_->capabilities = mojom::VRDisplayCapabilities::New();
  device_->capabilities->hasOrientation = true;
  device_->capabilities->hasPosition = false;
  device_->capabilities->hasExternalDisplay = false;
  device_->capabilities->canPresent = false;

  device_->leftEye = InitEye(45, -0.03f, 1024);
  device_->rightEye = InitEye(45, 0.03f, 1024);
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

  eye->offset = mojo::Array<float>::New(3);
  eye->offset[0] = offset;
  eye->offset[1] = 0.0f;
  eye->offset[2] = 0.0f;

  eye->renderWidth = size;
  eye->renderHeight = size;

  return eye;
}

void FakeVRDevice::SetVRDevice(const mojom::VRDisplayInfoPtr& device) {
  device_ = device.Clone();
}

void FakeVRDevice::SetPose(const mojom::VRPosePtr& pose) {
  pose_ = pose.Clone();
}

mojom::VRDisplayInfoPtr FakeVRDevice::GetVRDevice() {
  mojom::VRDisplayInfoPtr display = device_.Clone();
  return display.Clone();
}

mojom::VRPosePtr FakeVRDevice::GetPose(VRServiceImpl* service) {
  return pose_.Clone();
}

void FakeVRDevice::ResetPose(VRServiceImpl* service) {}

// TODO(shaobo.yan@intel.com): Will implemenate for VRDeviceServiceImpl tests.
bool FakeVRDevice::RequestPresent(VRServiceImpl* service, bool secure_origin) {
  return true;
}

void FakeVRDevice::ExitPresent(VRServiceImpl* service) {}

void FakeVRDevice::SubmitFrame(VRServiceImpl* service, mojom::VRPosePtr pose) {}

void FakeVRDevice::UpdateLayerBounds(VRServiceImpl* service,
                                     mojom::VRLayerBoundsPtr leftBounds,
                                     mojom::VRLayerBoundsPtr rightBounds) {}

}  // namespace device
