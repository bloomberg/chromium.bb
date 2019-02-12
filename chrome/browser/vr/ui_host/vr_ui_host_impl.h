// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_HOST_VR_UI_HOST_IMPL_H_
#define CHROME_BROWSER_VR_UI_HOST_VR_UI_HOST_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/vr/service/browser_xr_runtime.h"
#include "chrome/browser/vr/service/vr_ui_host.h"
#include "content/public/browser/web_contents.h"

namespace vr {

class VRBrowserRendererThreadWin;

// Concrete implementation of VRBrowserRendererHost, part of the "browser"
// component. Used on the browser's main thread.
class VRUiHostImpl : public VRUiHost,
                     public PermissionRequestManager::Observer,
                     public BrowserXRRuntimeObserver {
 public:
  VRUiHostImpl(device::mojom::XRDeviceId device_id,
               device::mojom::XRCompositorHostPtr compositor);
  ~VRUiHostImpl() override;

  // Factory for use with VRUiHost::{Set,Get}Factory
  static std::unique_ptr<VRUiHost> Create(
      device::mojom::XRDeviceId device_id,
      device::mojom::XRCompositorHostPtr compositor);

 private:
  // BrowserXRRuntimeObserver implementation.
  void SetWebXRWebContents(content::WebContents* contents) override;
  void SetVRDisplayInfo(device::mojom::VRDisplayInfoPtr display_info) override;

  // Internal methods used to start/stop the UI rendering thread that is used
  // for drawing browser UI (such as permission prompts) for display in VR.
  void StartUiRendering();
  void StopUiRendering();

  // PermissionRequestManager::Observer
  void OnBubbleAdded() override;
  void OnBubbleRemoved() override;

  void RemoveHeadsetNotificationPrompt(int prompt_sequence_num);

  device::mojom::XRCompositorHostPtr compositor_;
  std::unique_ptr<VRBrowserRendererThreadWin> ui_rendering_thread_;
  device::mojom::VRDisplayInfoPtr info_;
  content::WebContents* web_contents_ = nullptr;
  PermissionRequestManager* permission_request_manager_ = nullptr;

  bool is_prompt_showing_in_headset_ = false;
  int current_prompt_sequence_num_ = 0;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<VRUiHostImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VRUiHostImpl);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_HOST_VR_UI_HOST_IMPL_H_
