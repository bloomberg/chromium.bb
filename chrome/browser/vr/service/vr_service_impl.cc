// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/vr_service_impl.h"

#include <utility>

#include "base/bind.h"

#include "chrome/browser/vr/service/browser_xr_runtime.h"
#include "chrome/browser/vr/service/xr_device_impl.h"
#include "chrome/browser/vr/service/xr_runtime_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_display_impl.h"

namespace vr {

VRServiceImpl::VRServiceImpl(content::RenderFrameHost* render_frame_host)
    : WebContentsObserver(
          content::WebContents::FromRenderFrameHost(render_frame_host)),
      render_frame_host_(render_frame_host) {
  DCHECK(render_frame_host_);
  // TODO(crbug/701027): make sure that client_ is never null by initializing it
  // in the constructor.
}

// Constructor for testing.
VRServiceImpl::VRServiceImpl() : render_frame_host_(nullptr) {}

void VRServiceImpl::SetBinding(mojo::StrongBindingPtr<VRService> binding) {
  binding_ = std::move(binding);
}

VRServiceImpl::~VRServiceImpl() {
  // Destroy XRDeviceImpl before calling RemoveService below. RemoveService
  // might implicitly destory the XRRuntimeManager, and therefore the
  // BrowserXRRuntime that XRDeviceImpl needs to access in its dtor.
  device_ = nullptr;
  XRRuntimeManager::GetInstance()->RemoveService(this);
}

void VRServiceImpl::Create(content::RenderFrameHost* render_frame_host,
                           device::mojom::VRServiceRequest request) {
  std::unique_ptr<VRServiceImpl> vr_service_impl =
      std::make_unique<VRServiceImpl>(render_frame_host);
  VRServiceImpl* impl = vr_service_impl.get();
  impl->SetBinding(
      mojo::MakeStrongBinding(std::move(vr_service_impl), std::move(request)));
}

void VRServiceImpl::SetClient(device::mojom::VRServiceClientPtr service_client,
                              SetClientCallback callback) {
  DCHECK(!client_.get());
  client_ = std::move(service_client);
  set_client_callback_ = std::move(callback);

  // Once a client has been connected AddService will force any VRDisplays to
  // send ConnectDevice to it so that it's populated with the currently active
  // displays. Thereafter it will stay up to date by virtue of listening for new
  // connected events.
  XRRuntimeManager::GetInstance()->AddService(this);
}

void VRServiceImpl::InitializationComplete() {
  // device_ is added after all providers have initialized.  This means that we
  // can correctly answer SupportsSession, and can provide correct display
  // capabilities.
  device_ = std::make_unique<XRDeviceImpl>(render_frame_host_, client_.get());
  base::ResetAndReturn(&set_client_callback_).Run();
}

void VRServiceImpl::ConnectRuntime(BrowserXRRuntime* runtime) {
  // device_ is initialized on IntializationComplete.  Devices may be added
  // before that, but they'll be picked up during device_'s constructor.
  // We just need to notify device_ when new capabilities were added after
  // initialization.
  if (device_) {
    device_->OnRuntimeAvailable(runtime);
  }
}

void VRServiceImpl::RemoveRuntime(BrowserXRRuntime* runtime) {
  if (device_) {
    device_->OnRuntimeRemoved(runtime);
  }
}

void VRServiceImpl::SetListeningForActivate(bool listening) {
  if (device_)
    device_->SetListeningForActivate(listening);
}

void VRServiceImpl::OnWebContentsFocused(content::RenderWidgetHost* host) {
  OnWebContentsFocusChanged(host, true);
}

void VRServiceImpl::OnWebContentsLostFocus(content::RenderWidgetHost* host) {
  OnWebContentsFocusChanged(host, false);
}

void VRServiceImpl::RenderFrameDeleted(content::RenderFrameHost* host) {
  if (host != render_frame_host_)
    return;

  // Binding should always be live here, as this is a StrongBinding.
  // Close the binding (and delete this VrServiceImpl) when the RenderFrameHost
  // is deleted.
  DCHECK(binding_.get());
  binding_->Close();
}

void VRServiceImpl::OnWebContentsFocusChanged(content::RenderWidgetHost* host,
                                              bool focused) {
  if (!render_frame_host_->GetView() ||
      render_frame_host_->GetView()->GetRenderWidgetHost() != host) {
    return;
  }
  if (device_)
    device_->SetInFocusedFrame(focused);
}

}  // namespace vr
