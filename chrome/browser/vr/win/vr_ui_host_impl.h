// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_WIN_VR_UI_HOST_IMPL_H_
#define CHROME_BROWSER_VR_WIN_VR_UI_HOST_IMPL_H_

#include "base/threading/thread.h"
#include "chrome/browser/vr/service/browser_xr_runtime.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace vr {

class VRBrowserRendererThreadWin;

class VRUiHostImpl : public BrowserXRRuntimeObserver {
 public:
  // Called by IsolatedDeviceProvider when devices are added/removed.  These
  // manage the lifetime of VRBrowserRendererHostWin instances.
  static void AddCompositor(device::mojom::VRDisplayInfoPtr info,
                            device::mojom::XRCompositorHostPtr compositor);
  static void RemoveCompositor(device::mojom::XRDeviceId id);

 private:
  VRUiHostImpl(device::mojom::VRDisplayInfoPtr info,
               device::mojom::XRCompositorHostPtr compositor);
  ~VRUiHostImpl() override;

  // BrowserXRRuntimeObserver implementation.
  void SetWebXRWebContents(content::WebContents* contents) override;
  void SetVRDisplayInfo(device::mojom::VRDisplayInfoPtr display_info) override;

  // Internal methods used to start/stop the UI rendering thread that is used
  // for drawing browser UI (such as permission prompts) for display in VR.
  void StartUiRendering();
  void StopUiRendering();

  device::mojom::XRCompositorHostPtr compositor_;
  std::unique_ptr<VRBrowserRendererThreadWin> ui_rendering_thread_;
  device::mojom::VRDisplayInfoPtr info_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_WIN_VR_UI_HOST_IMPL_H_
