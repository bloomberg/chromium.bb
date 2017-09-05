// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device.h"

#include <math.h>
#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_delegate_provider.h"
#include "device/vr/android/gvr/gvr_delegate_provider_factory.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "device/vr/vr_device_manager.h"
#include "device/vr/vr_display_impl.h"
#include "jni/NonPresentingGvrContext_jni.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

std::unique_ptr<GvrDevice> GvrDevice::Create() {
  std::unique_ptr<GvrDevice> device = base::WrapUnique(new GvrDevice());
  if (!device->gvr_api_)
    return nullptr;
  return device;
}

GvrDevice::GvrDevice() : VRDevice() {
  GetGvrDelegateProvider();
  JNIEnv* env = base::android::AttachCurrentThread();
  non_presenting_context_.Reset(Java_NonPresentingGvrContext_create(env));
  if (!non_presenting_context_.obj())
    return;
  jlong context = Java_NonPresentingGvrContext_getNativeGvrContext(
      env, non_presenting_context_);
  gvr_api_ = gvr::GvrApi::WrapNonOwned(reinterpret_cast<gvr_context*>(context));
}

GvrDevice::~GvrDevice() {
  if (!non_presenting_context_.obj())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NonPresentingGvrContext_shutdown(env, non_presenting_context_);
}

void GvrDevice::CreateVRDisplayInfo(
    const base::Callback<void(mojom::VRDisplayInfoPtr)>& on_created) {
  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (delegate_provider) {
    delegate_provider->CreateVRDisplayInfo(gvr_api_.get(), on_created, id());
  } else {
    on_created.Run(nullptr);
  }
}

void GvrDevice::RequestPresent(mojom::VRSubmitFrameClientPtr submit_client,
                               mojom::VRPresentationProviderRequest request,
                               const base::Callback<void(bool)>& callback) {
  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (!delegate_provider)
    return callback.Run(false);

  // RequestWebVRPresent is async as we may trigger a DON flow that pauses
  // Chrome.
  delegate_provider->RequestWebVRPresent(std::move(submit_client),
                                         std::move(request), callback);
}

void GvrDevice::ExitPresent() {
  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (delegate_provider)
    delegate_provider->ExitWebVRPresent();
  OnExitPresent();
}

void GvrDevice::GetNextMagicWindowPose(
    VRDisplayImpl* display,
    mojom::VRDisplay::GetNextMagicWindowPoseCallback callback) {
  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (!delegate_provider) {
    std::move(callback).Run(nullptr);
    return;
  }
  delegate_provider->GetNextMagicWindowPose(gvr_api_.get(), display,
                                            std::move(callback));
}

void GvrDevice::OnDisplayAdded(VRDisplayImpl* display) {
  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (!delegate_provider)
    return;
  delegate_provider->OnDisplayAdded(display);
}

void GvrDevice::OnDisplayRemoved(VRDisplayImpl* display) {
  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (!delegate_provider)
    return;
  delegate_provider->OnDisplayRemoved(display);
}

void GvrDevice::OnListeningForActivateChanged(VRDisplayImpl* display) {
  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (!delegate_provider)
    return;
  delegate_provider->OnListeningForActivateChanged(display);
}

void GvrDevice::PauseTracking() {
  gvr_api_->PauseTracking();
}

void GvrDevice::ResumeTracking() {
  gvr_api_->ResumeTracking();
}

GvrDelegateProvider* GvrDevice::GetGvrDelegateProvider() {
  // GvrDelegateProviderFactory::Create() may fail transiently, so every time we
  // try to get it, set the device ID.
  GvrDelegateProvider* delegate_provider = GvrDelegateProviderFactory::Create();
  if (delegate_provider)
    delegate_provider->SetDeviceId(id());
  return delegate_provider;
}

}  // namespace device
