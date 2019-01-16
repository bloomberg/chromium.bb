// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_WIN_VR_BROWSER_RENDERER_THREAD_WIN_H_
#define CHROME_BROWSER_VR_WIN_VR_BROWSER_RENDERER_THREAD_WIN_H_

#include "base/threading/thread.h"
#include "chrome/browser/vr/browser_renderer.h"
#include "chrome/browser/vr/model/web_vr_model.h"
#include "chrome/browser/vr/service/browser_xr_runtime.h"
#include "chrome/browser/vr/vr_export.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace vr {

class InputDelegateWin;
class GraphicsDelegateWin;
class SchedulerDelegateWin;
class VRUiBrowserInterface;

class VR_EXPORT VRBrowserRendererThreadWin {
 public:
  VRBrowserRendererThreadWin();
  ~VRBrowserRendererThreadWin();

  void StartOverlay(device::mojom::XRCompositorHost* host);
  void StopOverlay();
  void SetVRDisplayInfo(device::mojom::VRDisplayInfoPtr display_info);
  void SetLocationInfo(GURL gurl);
  void SetVisibleExternalPromptNotification(
      ExternalPromptNotificationType prompt);

  static VRBrowserRendererThreadWin* GetInstanceForTesting();
  BrowserRenderer* GetBrowserRendererForTesting();

 private:
  void CleanUp();
  void OnPose(device::mojom::XRFrameDataPtr data);
  void SubmitResult(bool success);
  void SubmitFrame(device::mojom::XRFrameDataPtr data);
  // If there is fullscreen UI in-headset, we won't composite WebXR content or
  // render WebXR Overlays. We can even avoid giving out poses to avoid spending
  // resources drawing things that won't be shown.
  // When we return false, we tell the Ui to DrawWebVR. When we return true, we
  // tell the Ui to DrawUI.
  bool ShouldPauseWebXrAndDrawUI();

  // We need to do some initialization of GraphicsDelegateWin before
  // browser_renderer_, so we first store it in a unique_ptr, then transition
  // ownership to browser_renderer_.
  std::unique_ptr<GraphicsDelegateWin> initializing_graphics_;
  std::unique_ptr<VRUiBrowserInterface> ui_browser_interface_;
  std::unique_ptr<BrowserRenderer> browser_renderer_;

  // Raw pointers to objects owned by browser_renderer_:
  InputDelegateWin* input_ = nullptr;
  GraphicsDelegateWin* graphics_ = nullptr;
  SchedulerDelegateWin* scheduler_ = nullptr;
  BrowserUiInterface* ui_ = nullptr;
  ExternalPromptNotificationType current_external_prompt_notification_type_ =
      ExternalPromptNotificationType::kPromptNone;

  device::mojom::ImmersiveOverlayPtr overlay_;
  device::mojom::VRDisplayInfoPtr display_info_;

  // This class is effectively a singleton, although it's not actually
  // implemented as one. Since tests need to access the thread to post tasks,
  // just keep a static reference to the existing instance.
  static VRBrowserRendererThreadWin* instance_for_testing_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_WIN_VR_BROWSER_RENDERER_THREAD_WIN_H_
