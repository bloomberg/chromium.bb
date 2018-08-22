// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_RENDER_LOOP_H_
#define CHROME_BROWSER_VR_RENDER_LOOP_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/vr/compositor_delegate.h"
#include "chrome/browser/vr/gl_texture_location.h"
#include "chrome/browser/vr/sliding_average.h"
#include "chrome/browser/vr/vr_export.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace base {
class TimeDelta;
class TimeTicks;
}

namespace vr {

enum class VrUiTestActivityResult;
class BrowserUiInterface;
class ControllerDelegate;
class PlatformInputHandler;
class PlatformUiInputDelegate;
class RenderLoopBrowserInterface;
class SchedulerDelegate;
class UiInterface;
struct ControllerTestInput;
struct RenderInfo;
struct UiTestActivityExpectation;
struct UiTestState;

// This abstract class handles all input/output activities during a frame.
// This includes head movement, controller movement and input, audio output and
// rendering of the frame.
// TODO(acondor): Move more functionality cross platform functionality from
// VrShellGl and make this class concrete (http://crbug.com/767282).
class VR_EXPORT RenderLoop {
 public:
  RenderLoop(std::unique_ptr<UiInterface> ui,
             CompositorDelegate* compositor_delegate,
             SchedulerDelegate* scheduler_delegate,
             std::unique_ptr<ControllerDelegate> controller_delegate,
             RenderLoopBrowserInterface* browser,
             size_t sliding_time_size);
  virtual ~RenderLoop();

  void Draw(CompositorDelegate::FrameType frame_type,
            base::TimeTicks current_time);

  void OnPause();
  void OnResume();

  void OnSwapContents(int new_content_id);
  void EnableAlertDialog(PlatformInputHandler* input_handler,
                         float width,
                         float height);
  void DisableAlertDialog();
  void SetAlertDialogSize(float width, float height);
  void SetDialogLocation(float x, float y);
  void SetDialogFloating(bool floating);
  void ShowToast(const base::string16& text);
  void CancelToast();
  void ContentBoundsChanged(int width, int height);

  base::WeakPtr<BrowserUiInterface> GetBrowserUiWeakPtr();

  void PerformControllerActionForTesting(ControllerTestInput controller_input);
  void SetUiExpectingActivityForTesting(
      UiTestActivityExpectation ui_expectation);
  void AcceptDoffPromptForTesting();

 protected:
  device::mojom::XRInputSourceStatePtr ProcessControllerInputForWebXr(
      const RenderInfo& render_info,
      base::TimeTicks current_time);
  void ForceExitVr();

  const SlidingTimeDeltaAverage& ui_controller_update_time() const {
    return ui_controller_update_time_;
  }
  const SlidingTimeDeltaAverage& ui_processing_time() const {
    return ui_processing_time_;
  }

 private:
  // Position, hide and/or show UI elements, process input and update textures.
  // Returns true if the scene changed.
  void UpdateUi(const RenderInfo& render_info,
                base::TimeTicks currrent_time,
                CompositorDelegate::FrameType frame_type);
  void DrawWebXr();
  void DrawWebXrOverlay(const RenderInfo& render_info);
  void DrawContentQuad();
  void DrawBrowserUi(const RenderInfo& render_info);
  base::TimeDelta ProcessControllerInput(const RenderInfo& render_info,
                                         base::TimeTicks current_time);

  void ReportUiStatusForTesting(const base::TimeTicks& current_time,
                                bool ui_updated);
  void ReportUiActivityResultForTesting(VrUiTestActivityResult result);

  std::unique_ptr<UiInterface> ui_;
  CompositorDelegate* compositor_delegate_;
  SchedulerDelegate* scheduler_delegate_;
  std::unique_ptr<ControllerDelegate> controller_delegate_;
  std::unique_ptr<ControllerDelegate> controller_delegate_for_testing_;
  bool using_controller_delegate_for_testing_ = false;

  std::unique_ptr<PlatformUiInputDelegate> vr_dialog_input_delegate_;

  RenderLoopBrowserInterface* browser_;

  std::unique_ptr<UiTestState> ui_test_state_;
  SlidingTimeDeltaAverage ui_processing_time_;
  SlidingTimeDeltaAverage ui_controller_update_time_;

  DISALLOW_COPY_AND_ASSIGN(RenderLoop);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_RENDER_LOOP_H_
