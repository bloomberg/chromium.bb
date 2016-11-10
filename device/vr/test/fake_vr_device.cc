// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/fake_vr_device.h"

namespace device {

FakeVRDevice::FakeVRDevice(VRDeviceProvider* provider) : VRDevice(provider) {
  device_ = VRDisplay::New();
  pose_ = VRPose::New();

  InitBasicDevice();
}

FakeVRDevice::~FakeVRDevice() {}

void FakeVRDevice::InitBasicDevice() {
  device_->displayName = "FakeVRDevice";

  device_->capabilities = VRDisplayCapabilities::New();
  device_->capabilities->hasOrientation = true;
  device_->capabilities->hasPosition = false;
  device_->capabilities->hasExternalDisplay = false;
  device_->capabilities->canPresent = false;

  device_->leftEye = InitEye(45, -0.03f, 1024);
  device_->rightEye = InitEye(45, 0.03f, 1024);
}

VREyeParametersPtr FakeVRDevice::InitEye(float fov,
                                         float offset,
                                         uint32_t size) {
  VREyeParametersPtr eye = VREyeParameters::New();

  eye->fieldOfView = VRFieldOfView::New();
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

void FakeVRDevice::SetVRDevice(const VRDisplayPtr& device) {
  device_ = device.Clone();
}

void FakeVRDevice::SetPose(const VRPosePtr& pose) {
  pose_ = pose.Clone();
}

VRDisplayPtr FakeVRDevice::GetVRDevice() {
  VRDisplayPtr display = device_.Clone();
  display->index = id();
  return display.Clone();
}

VRPosePtr FakeVRDevice::GetPose() {
  return pose_.Clone();
}

void FakeVRDevice::ResetPose() {}

}  // namespace device
