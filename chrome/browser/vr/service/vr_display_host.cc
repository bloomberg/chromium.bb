// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/vr_display_host.h"

#include "base/bind.h"
#include "device/vr/vr_display_impl.h"

namespace vr {

VRDisplayHost::VRDisplayHost(device::VRDevice* device,
                             int render_frame_process_id,
                             int render_frame_routing_id,
                             device::mojom::VRServiceClient* service_client,
                             device::mojom::VRDisplayInfoPtr display_info)
    : binding_(this) {
  device::mojom::VRDisplayHostPtr display;
  binding_.Bind(mojo::MakeRequest(&display));
  display_ = std::make_unique<device::VRDisplayImpl>(
      device, render_frame_process_id, render_frame_routing_id,
      std::move(service_client), std::move(display_info), std::move(display));
}

VRDisplayHost::~VRDisplayHost() = default;

void VRDisplayHost::RequestPresent(
    device::mojom::VRSubmitFrameClientPtr client,
    device::mojom::VRPresentationProviderRequest request,
    RequestPresentCallback callback) {
  display_->RequestPresent(std::move(client), std::move(request),
                           std::move(callback));
}

void VRDisplayHost::ExitPresent() {
  display_->ExitPresent();
}

void VRDisplayHost::SetListeningForActivate(bool listening) {
  display_->SetListeningForActivate(listening);
}

}  // namespace vr
