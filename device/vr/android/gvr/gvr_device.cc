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
#include "ui/gfx/geometry/rect_f.h"
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

void GvrDevice::RequestPresent(mojom::VRSubmitFrameClientPtr submit_client,
                               const base::Callback<void(bool)>& callback) {
  GvrDelegateProvider* delegate_provider = gvr_provider_->GetDelegateProvider();
  if (!delegate_provider)
    return callback.Run(false);

  // RequestWebVRPresent is async as we may trigger a DON flow that pauses
  // Chrome.
  delegate_provider->RequestWebVRPresent(std::move(submit_client), callback);
}

void GvrDevice::SetSecureOrigin(bool secure_origin) {
  secure_origin_ = secure_origin;
  GvrDelegate* delegate = GetGvrDelegate();
  if (delegate)
    delegate->SetWebVRSecureOrigin(secure_origin_);
}

void GvrDevice::ExitPresent() {
  GvrDelegateProvider* delegate_provider = gvr_provider_->GetDelegateProvider();
  if (delegate_provider)
    delegate_provider->ExitWebVRPresent();
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
                                  mojom::VRLayerBoundsPtr left_bounds_ptr,
                                  mojom::VRLayerBoundsPtr right_bounds_ptr,
                                  int16_t source_width,
                                  int16_t source_height) {
  GvrDelegate* delegate = GetGvrDelegate();
  if (!delegate)
    return;

  gfx::RectF left_bounds(left_bounds_ptr->left, left_bounds_ptr->top,
                         left_bounds_ptr->width, left_bounds_ptr->height);
  gfx::RectF right_bounds(right_bounds_ptr->left, right_bounds_ptr->top,
                          right_bounds_ptr->width, right_bounds_ptr->height);
  gfx::Size source_size(source_width, source_height);
  delegate->UpdateWebVRTextureBounds(frame_index, left_bounds, right_bounds,
                                     source_size);
}

void GvrDevice::GetVRVSyncProvider(mojom::VRVSyncProviderRequest request) {
  GvrDelegate* delegate = GetGvrDelegate();
  if (delegate)
    delegate->OnVRVsyncProviderRequest(std::move(request));
}

void GvrDevice::OnDelegateChanged() {
  GvrDelegate* delegate = GetGvrDelegate();
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
