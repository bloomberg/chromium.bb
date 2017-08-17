// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device_provider.h"

#include "device/vr/android/gvr/gvr_delegate_provider.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/vr_device.h"

namespace device {

GvrDeviceProvider::GvrDeviceProvider()
    : vr_device_(base::MakeUnique<GvrDevice>(this)) {}

GvrDeviceProvider::~GvrDeviceProvider() {
  GvrDelegateProvider* delegate_provider = GvrDelegateProvider::GetInstance();
  if (delegate_provider) {
    delegate_provider->ExitWebVRPresent();
    delegate_provider->ClearDeviceProvider();
  }
}

void GvrDeviceProvider::GetDevices(std::vector<VRDevice*>* devices) {
  devices->push_back(vr_device_.get());
}

GvrDelegateProvider* GvrDeviceProvider::GetDelegateProvider() {
  GvrDelegateProvider* provider = GvrDelegateProvider::GetInstance();
  Initialize(provider);
  return provider;
}

void GvrDeviceProvider::Initialize() {
  Initialize(GvrDelegateProvider::GetInstance());
}

void GvrDeviceProvider::Initialize(GvrDelegateProvider* provider) {
  if (!provider)
    return;
  provider->SetDeviceProvider(this);
}

}  // namespace device
