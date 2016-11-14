// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device_provider.h"

#include <jni.h>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "base/android/scoped_java_ref.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/android/gvr/gvr_gamepad_data_fetcher.h"
#include "device/vr/vr_device_manager.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr_controller.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr_types.h"

using base::android::AttachCurrentThread;
using base::android::GetApplicationContext;

namespace device {

GvrDeviceProvider::GvrDeviceProvider() : weak_ptr_factory_(this) {}

GvrDeviceProvider::~GvrDeviceProvider() {
  device::GvrDelegateProvider* delegate_provider =
      device::GvrDelegateProvider::GetInstance();
  if (delegate_provider)
    delegate_provider->DestroyNonPresentingDelegate();

  ExitPresent();
}

void GvrDeviceProvider::GetDevices(std::vector<VRDevice*>* devices) {
  Initialize();

  if (vr_device_)
    devices->push_back(vr_device_.get());
}

void GvrDeviceProvider::Initialize() {
  device::GvrDelegateProvider* delegate_provider =
      device::GvrDelegateProvider::GetInstance();
  if (!delegate_provider)
    return;

  if (!vr_device_) {
    vr_device_.reset(
        new GvrDevice(this, delegate_provider->GetNonPresentingDelegate()));
  }
}

bool GvrDeviceProvider::RequestPresent() {
  device::GvrDelegateProvider* delegate_provider =
      device::GvrDelegateProvider::GetInstance();
  if (!delegate_provider)
    return false;

  // RequestWebVRPresent is async as a render thread may be created.
  return delegate_provider->RequestWebVRPresent(weak_ptr_factory_.GetWeakPtr());
}

void GvrDeviceProvider::ExitPresent() {
  if (!vr_device_)
    return;

  device::GvrDelegateProvider* delegate_provider =
      device::GvrDelegateProvider::GetInstance();
  if (!delegate_provider)
    return;

  vr_device_->SetDelegate(delegate_provider->GetNonPresentingDelegate());

  GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
      GAMEPAD_SOURCE_GVR);

  delegate_provider->ExitWebVRPresent();
}

void GvrDeviceProvider::OnGvrDelegateReady(
    const base::WeakPtr<GvrDelegate>& delegate) {
  if (!vr_device_)
    return;
  vr_device_->SetDelegate(delegate);
  GamepadDataFetcherManager::GetInstance()->AddFactory(
      new GvrGamepadDataFetcher::Factory(delegate, vr_device_->id()));
}

void GvrDeviceProvider::OnGvrDelegateRemoved() {
  ExitPresent();
}

void GvrDeviceProvider::OnDisplayBlur() {
  if (!vr_device_)
    return;
  vr_device_->OnDisplayBlur();
}

void GvrDeviceProvider::OnDisplayFocus() {
  if (!vr_device_)
    return;
  vr_device_->OnDisplayFocus();
}

}  // namespace device
