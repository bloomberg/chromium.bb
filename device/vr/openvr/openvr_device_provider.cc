// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/openvr_device_provider.h"
#include "device/vr/openvr/openvr_device.h"
#include "third_party/openvr/src/headers/openvr.h"

namespace device {

OpenVRDeviceProvider::OpenVRDeviceProvider() {}

OpenVRDeviceProvider::~OpenVRDeviceProvider() {}

void OpenVRDeviceProvider::GetDevices(std::vector<VRDevice*>* devices) {
  if (vr::VR_IsRuntimeInstalled() && vr::VR_IsHmdPresent()) {
    devices->push_back(new OpenVRDevice());
  }
}

void OpenVRDeviceProvider::Initialize() {}

void OpenVRDeviceProvider::SetListeningForActivate(bool listening) {}

}  // namespace device