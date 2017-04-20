// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device_provider.h"

#include <jni.h>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "base/android/scoped_java_ref.h"
#include "device/vr/android/gvr/gvr_delegate_provider.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_manager.h"
#include "device/vr/vr_service.mojom.h"

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

void GvrDeviceProvider::RequestPresent(
    mojom::VRSubmitFrameClientPtr submit_client,
    const base::Callback<void(bool)>& callback) {
  GvrDelegateProvider* delegate_provider = GetDelegateProvider();
  if (!delegate_provider)
    return callback.Run(false);

  // RequestWebVRPresent is async as we may trigger a DON flow that pauses
  // Chrome.
  delegate_provider->RequestWebVRPresent(std::move(submit_client), callback);
}

// VR presentation exit requested by the API.
void GvrDeviceProvider::ExitPresent() {
  GvrDelegateProvider* delegate_provider = GetDelegateProvider();
  if (delegate_provider)
    delegate_provider->ExitWebVRPresent();
}

void GvrDeviceProvider::SetListeningForActivate(bool listening) {
  GvrDelegateProvider* delegate_provider = GetDelegateProvider();
  if (!delegate_provider)
    return;

  delegate_provider->SetListeningForActivate(listening);
}

}  // namespace device
