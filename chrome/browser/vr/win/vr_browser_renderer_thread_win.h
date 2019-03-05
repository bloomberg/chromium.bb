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
  explicit VRBrowserRendererThreadWin(
      device::mojom::XRCompositorHost* compositor);
  ~VRBrowserRendererThreadWin();

  void SetVRDisplayInfo(device::mojom::VRDisplayInfoPtr display_info);
  void SetLocationInfo(GURL gurl);

  // The below function(s) affect(s) whether UI is drawn or not.
  void SetVisibleExternalPromptNotification(
      ExternalPromptNotificationType prompt);

  static VRBrowserRendererThreadWin* GetInstanceForTesting();
  BrowserRenderer* GetBrowserRendererForTesting();

 private:
  class DrawState {
   public:
    // State changing methods.
    bool SetPrompt(ExternalPromptNotificationType prompt) {
      auto old = prompt_;
      prompt_ = prompt;
      return prompt_ != old;
    }

    // State querying methods.
    bool ShouldDrawUI();
    bool ShouldDrawWebXR();

   private:
    ExternalPromptNotificationType prompt_ =
        ExternalPromptNotificationType::kPromptNone;
  };

  void OnPose(device::mojom::XRFrameDataPtr data);
  void SubmitResult(bool success);
  void SubmitFrame(device::mojom::XRFrameDataPtr data);
  void StartOverlay();
  void StopOverlay();

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

  // Owned by vr_ui_host:
  device::mojom::XRCompositorHost* compositor_;

  GURL gurl_;
  DrawState draw_state_;

  device::mojom::ImmersiveOverlayPtr overlay_;
  device::mojom::VRDisplayInfoPtr display_info_;

  // This class is effectively a singleton, although it's not actually
  // implemented as one. Since tests need to access the thread to post tasks,
  // just keep a static reference to the existing instance.
  static VRBrowserRendererThreadWin* instance_for_testing_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_WIN_VR_BROWSER_RENDERER_THREAD_WIN_H_
