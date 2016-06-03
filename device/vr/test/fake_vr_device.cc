// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/fake_vr_device.h"

namespace device {

FakeVRDevice::FakeVRDevice(VRDeviceProvider* provider) : VRDevice(provider) {
  device_ = blink::mojom::VRDisplay::New();
  pose_ = blink::mojom::VRPose::New();
}

FakeVRDevice::~FakeVRDevice() {}

void FakeVRDevice::SetVRDevice(const blink::mojom::VRDisplayPtr& device) {
  device_ = device.Clone();
}

void FakeVRDevice::SetPose(const blink::mojom::VRPosePtr& pose) {
  pose_ = pose.Clone();
}

blink::mojom::VRDisplayPtr FakeVRDevice::GetVRDevice() {
  return device_.Clone();
}

blink::mojom::VRPosePtr FakeVRDevice::GetPose() {
  return pose_.Clone();
}

void FakeVRDevice::ResetPose() {}

}  // namespace device
