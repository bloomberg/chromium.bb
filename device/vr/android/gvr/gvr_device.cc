// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device.h"

#include <math.h>
#include <algorithm>

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_delegate_provider.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "device/vr/vr_device_manager.h"
#include "device/vr/vr_display_impl.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

GvrDevice::GvrDevice() : VRDevice() {
  GetGvrDelegateProvider();
}

GvrDevice::~GvrDevice() {}

void GvrDevice::CreateVRDisplayInfo(
    const base::Callback<void(mojom::VRDisplayInfoPtr)>& on_created) {
  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (delegate_provider) {
    delegate_provider->CreateVRDisplayInfo(on_created, id());
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
  delegate_provider->GetNextMagicWindowPose(display, std::move(callback));
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

GvrDelegateProvider* GvrDevice::GetGvrDelegateProvider() {
  // GvrDelegateProvider::GetInstance() may fail transiently, so every time we
  // try to get it, set the device ID.
  GvrDelegateProvider* delegate_provider = GvrDelegateProvider::GetInstance();
  if (delegate_provider)
    delegate_provider->SetDeviceId(id());
  return delegate_provider;
}

}  // namespace device
