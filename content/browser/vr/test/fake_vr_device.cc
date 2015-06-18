// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/vr/test/fake_vr_device.h"

namespace content {

FakeVRDevice::FakeVRDevice(VRDeviceProvider* provider) : VRDevice(provider) {
  device_ = VRDeviceInfo::New();
  state_ = VRSensorState::New();
}

FakeVRDevice::~FakeVRDevice() {
}

void FakeVRDevice::SetVRDevice(const VRDeviceInfoPtr& device) {
  device_ = device.Clone();
}

void FakeVRDevice::SetSensorState(const VRSensorStatePtr& state) {
  state_ = state.Clone();
}

VRDeviceInfoPtr FakeVRDevice::GetVRDevice() {
  return device_.Clone();
}

VRSensorStatePtr FakeVRDevice::GetSensorState() {
  return state_.Clone();
}

}  // namespace content