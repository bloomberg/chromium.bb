// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_host/vr_ui_host_impl.h"

#include "chrome/browser/vr/service/browser_xr_runtime.h"
#include "chrome/browser/vr/service/xr_runtime_manager.h"
#include "chrome/browser/vr/win/vr_browser_renderer_thread_win.h"

namespace vr {

VRUiHostImpl::VRUiHostImpl(device::mojom::VRDisplayInfoPtr info,
                           device::mojom::XRCompositorHostPtr compositor)
    : compositor_(std::move(compositor)), info_(std::move(info)) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

  BrowserXRRuntime* runtime =
      XRRuntimeManager::GetInstance()->GetRuntime(info_->id);
  if (runtime) {
    runtime->AddObserver(this);
  }
}

VRUiHostImpl::~VRUiHostImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

  // We don't call BrowserXRRuntime::RemoveObserver, because if we are being
  // destroyed, it means the corresponding device has been removed from
  // XRRuntimeManager, and the BrowserXRRuntime has been destroyed.

  StopUiRendering();
}

// static
std::unique_ptr<VRUiHost> VRUiHostImpl::Create(
    device::mojom::VRDisplayInfoPtr info,
    device::mojom::XRCompositorHostPtr compositor) {
  DVLOG(1) << __func__;
  return std::make_unique<VRUiHostImpl>(std::move(info), std::move(compositor));
}

void VRUiHostImpl::SetWebXRWebContents(content::WebContents* contents) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Eventually the contents will be used to poll for permissions, or determine
  // what overlays should show.
  if (contents)
    StartUiRendering();
  else
    StopUiRendering();
}

void VRUiHostImpl::SetVRDisplayInfo(
    device::mojom::VRDisplayInfoPtr display_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

  info_ = std::move(display_info);
  if (ui_rendering_thread_) {
    ui_rendering_thread_->SetVRDisplayInfo(info_.Clone());
  }
}

void VRUiHostImpl::StartUiRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

// Only used for testing currently.  To see an example overlay, enable the
// following few lines. TODO(https://crbug.com/911734): show browser UI for
// permission prompts in VR here.
#if 0
  ui_rendering_thread_ = std::make_unique<VRBrowserRendererThreadWin>();
  ui_rendering_thread_->Start();
  ui_rendering_thread_->SetVRDisplayInfo(info_.Clone());
  ui_rendering_thread_->StartOverlay(compositor_.get());
#endif
}

void VRUiHostImpl::StopUiRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

  ui_rendering_thread_ = nullptr;
}

}  // namespace vr
