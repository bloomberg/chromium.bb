// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/vr/test/fake_vr_device.h"

namespace content {

FakeVRDevice::FakeVRDevice(VRDeviceProvider* provider) : VRDevice(provider) {
  device_ = blink::mojom::VRDisplay::New();
  state_ = blink::mojom::VRPose::New();
}

FakeVRDevice::~FakeVRDevice() {
}

void FakeVRDevice::SetVRDevice(const blink::mojom::VRDisplayPtr& device) {
  device_ = device.Clone();
}

void FakeVRDevice::SetSensorState(const blink::mojom::VRPosePtr& state) {
  state_ = state.Clone();
}

blink::mojom::VRDisplayPtr FakeVRDevice::GetVRDevice() {
  return device_.Clone();
}

blink::mojom::VRPosePtr FakeVRDevice::GetSensorState() {
  return state_.Clone();
}

}  // namespace content