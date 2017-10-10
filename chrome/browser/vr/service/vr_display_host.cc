// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/vr_display_host.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "device/vr/vr_display_impl.h"

namespace vr {

namespace {

// TODO(mthiesse): When we unship WebVR 1.1, set this to false.
static constexpr bool kAllowHTTPWebVRWithFlag = true;

bool IsSecureContext(content::RenderFrameHost* host) {
  if (!host)
    return false;
  while (host != nullptr) {
    if (!content::IsOriginSecure(host->GetLastCommittedURL()))
      return false;
    host = host->GetParent();
  }
  return true;
}

}  // namespace

VRDisplayHost::VRDisplayHost(device::VRDevice* device,
                             content::RenderFrameHost* render_frame_host,
                             device::mojom::VRServiceClient* service_client,
                             device::mojom::VRDisplayInfoPtr display_info)
    : render_frame_host_(render_frame_host), binding_(this) {
  device::mojom::VRDisplayHostPtr display;
  binding_.Bind(mojo::MakeRequest(&display));
  display_ = std::make_unique<device::VRDisplayImpl>(
      device, std::move(service_client), std::move(display_info),
      std::move(display),
      render_frame_host ? render_frame_host->GetView()->HasFocus() : false);
}

VRDisplayHost::~VRDisplayHost() = default;

void VRDisplayHost::RequestPresent(
    device::mojom::VRSubmitFrameClientPtr client,
    device::mojom::VRPresentationProviderRequest request,
    RequestPresentCallback callback) {
  bool requires_secure_context =
      !kAllowHTTPWebVRWithFlag ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebVR);
  if (requires_secure_context && !IsSecureContext(render_frame_host_)) {
    std::move(callback).Run(false);
    return;
  }
  display_->RequestPresent(std::move(client), std::move(request),
                           std::move(callback));
}

void VRDisplayHost::ExitPresent() {
  display_->ExitPresent();
}

void VRDisplayHost::SetListeningForActivate(bool listening) {
  display_->SetListeningForActivate(listening);
}

void VRDisplayHost::SetInFocusedFrame(bool in_focused_frame) {
  display_->SetInFocusedFrame(in_focused_frame);
}

}  // namespace vr
