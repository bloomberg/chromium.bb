// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device_provider.h"

#include "device/vr/android/gvr/gvr_delegate_provider.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/vr_device.h"

namespace device {

GvrDeviceProvider::GvrDeviceProvider() = default;
GvrDeviceProvider::~GvrDeviceProvider() = default;

void GvrDeviceProvider::GetDevices(std::vector<VRDevice*>* devices) {
  devices->push_back(vr_device_.get());
}

void GvrDeviceProvider::Initialize() {
  vr_device_ = base::MakeUnique<GvrDevice>();
}

}  // namespace device
