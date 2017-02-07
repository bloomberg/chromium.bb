// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device.h"

#include <math.h>
#include <algorithm>

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "device/vr/vr_device_manager.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

GvrDevice::GvrDevice(GvrDeviceProvider* provider)
    : VRDevice(), gvr_provider_(provider) {}

GvrDevice::~GvrDevice() {}

void GvrDevice::GetVRDevice(
    const base::Callback<void(mojom::VRDisplayInfoPtr)>& callback) {
  GvrDelegate* delegate = GetGvrDelegate();
  if (delegate) {
    delegate->CreateVRDisplayInfo(callback, id());
  } else {
    callback.Run(mojom::VRDisplayInfoPtr(nullptr));
  }
}

void GvrDevice::ResetPose() {
  GvrDelegate* delegate = GetGvrDelegate();
  if (delegate)
    delegate->ResetPose();
}

void GvrDevice::RequestPresent(const base::Callback<void(bool)>& callback) {
  gvr_provider_->RequestPresent(callback);
}

void GvrDevice::SetSecureOrigin(bool secure_origin) {
  secure_origin_ = secure_origin;
  GvrDelegate* delegate = GetGvrDelegate();
  if (delegate)
    delegate->SetWebVRSecureOrigin(secure_origin_);
}

void GvrDevice::ExitPresent() {
  gvr_provider_->ExitPresent();
  OnExitPresent();
}

void GvrDevice::SubmitFrame(mojom::VRPosePtr pose) {
  GvrDelegate* delegate = GetGvrDelegate();
  if (delegate)
    delegate->SubmitWebVRFrame();
}

void GvrDevice::UpdateLayerBounds(int16_t frame_index,
                                  mojom::VRLayerBoundsPtr left_bounds,
                                  mojom::VRLayerBoundsPtr right_bounds) {
  GvrDelegate* delegate = GetGvrDelegate();
  if (!delegate)
    return;

  gvr::Rectf left_gvr_bounds;
  left_gvr_bounds.left = left_bounds->left;
  left_gvr_bounds.top = 1.0f - left_bounds->top;
  left_gvr_bounds.right = left_bounds->left + left_bounds->width;
  left_gvr_bounds.bottom = 1.0f - (left_bounds->top + left_bounds->height);

  gvr::Rectf right_gvr_bounds;
  right_gvr_bounds.left = right_bounds->left;
  right_gvr_bounds.top = 1.0f - right_bounds->top;
  right_gvr_bounds.right = right_bounds->left + right_bounds->width;
  right_gvr_bounds.bottom = 1.0f - (right_bounds->top + right_bounds->height);

  delegate->UpdateWebVRTextureBounds(frame_index, left_gvr_bounds,
                                     right_gvr_bounds);
}

void GvrDevice::GetVRVSyncProvider(mojom::VRVSyncProviderRequest request) {
  GvrDelegate* delegate = GetGvrDelegate();
  if (delegate)
    delegate->OnVRVsyncProviderRequest(std::move(request));
}

void GvrDevice::OnDelegateChanged() {
  GvrDelegate* delegate = GetGvrDelegate();
  if (!delegate || !delegate->SupportsPresentation())
    OnExitPresent();
  // Notify the clients that this device has changed
  if (delegate)
    delegate->SetWebVRSecureOrigin(secure_origin_);

  OnChanged();
}

GvrDelegate* GvrDevice::GetGvrDelegate() {
  GvrDelegateProvider* delegate_provider = GvrDelegateProvider::GetInstance();
  if (delegate_provider)
    return delegate_provider->GetDelegate();
  return nullptr;
}

}  // namespace device
