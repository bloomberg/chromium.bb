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

void GvrDevice::CreateVRDisplayInfo(
    const base::Callback<void(mojom::VRDisplayInfoPtr)>& on_created) {
  GvrDelegate* delegate = GetGvrDelegate();
  if (delegate) {
    delegate->CreateVRDisplayInfo(on_created, id());
  } else {
    on_created.Run(nullptr);
  }
}

void GvrDevice::ResetPose() {
  GvrDelegate* delegate = GetGvrDelegate();
  if (delegate)
    delegate->ResetPose();
}

void GvrDevice::RequestPresent(mojom::VRSubmitFrameClientPtr submit_client,
                               const base::Callback<void(bool)>& callback) {
  gvr_provider_->RequestPresent(std::move(submit_client), callback);
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

void GvrDevice::SubmitFrame(int16_t frame_index,
                            const gpu::MailboxHolder& mailbox) {
  GvrDelegate* delegate = GetGvrDelegate();
  if (delegate) {
    delegate->SubmitWebVRFrame(frame_index, mailbox);
  }
}

void GvrDevice::UpdateLayerBounds(int16_t frame_index,
                                  mojom::VRLayerBoundsPtr left_bounds,
                                  mojom::VRLayerBoundsPtr right_bounds,
                                  int16_t source_width,
                                  int16_t source_height) {
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

  gvr::Sizei source_size = {source_width, source_height};
  delegate->UpdateWebVRTextureBounds(frame_index, left_gvr_bounds,
                                     right_gvr_bounds, source_size);
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
  GvrDelegateProvider* delegate_provider = gvr_provider_->GetDelegateProvider();
  if (delegate_provider)
    return delegate_provider->GetDelegate();
  return nullptr;
}

}  // namespace device
