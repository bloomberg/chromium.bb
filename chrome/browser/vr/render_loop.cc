// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/render_loop.h"

#include <utility>

#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "chrome/browser/vr/controller_delegate_for_testing.h"
#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/model/reticle_model.h"
#include "chrome/browser/vr/platform_ui_input_delegate.h"
#include "chrome/browser/vr/render_info.h"
#include "chrome/browser/vr/render_loop_browser_interface.h"
#include "chrome/browser/vr/scheduler_delegate.h"
#include "chrome/browser/vr/ui_interface.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "ui/gl/gl_bindings.h"

namespace vr {

RenderLoop::RenderLoop(std::unique_ptr<UiInterface> ui,
                       CompositorDelegate* compositor_delegate,
                       SchedulerDelegate* scheduler_delegate,
                       std::unique_ptr<ControllerDelegate> controller_delegate,
                       RenderLoopBrowserInterface* browser,
                       size_t sliding_time_size)
    : ui_(std::move(ui)),
      compositor_delegate_(compositor_delegate),
      scheduler_delegate_(scheduler_delegate),
      controller_delegate_(std::move(controller_delegate)),
      browser_(browser),
      ui_processing_time_(sliding_time_size),
      ui_controller_update_time_(sliding_time_size) {
  compositor_delegate_->SetUiInterface(ui_.get());
}

RenderLoop::~RenderLoop() = default;

void RenderLoop::Draw(CompositorDelegate::FrameType frame_type,
                      base::TimeTicks current_time) {
  TRACE_EVENT1("gpu", __func__, "frame_type", frame_type);
  const auto& render_info = compositor_delegate_->GetRenderInfo(frame_type);
  UpdateUi(render_info, current_time, frame_type);
  ui_->OnProjMatrixChanged(render_info.left_eye_model.proj_matrix);
  bool use_quad_layer = ui_->IsContentVisibleAndOpaque() &&
                        compositor_delegate_->IsContentQuadReady();
  ui_->SetContentUsesQuadLayer(use_quad_layer);

  compositor_delegate_->InitializeBuffers();
  if (frame_type == CompositorDelegate::kWebXrFrame) {
    DCHECK(!use_quad_layer);
    DrawWebXr();
    if (ui_->HasWebXrOverlayElementsToDraw())
      DrawWebXrOverlay(render_info);
  } else {
    if (use_quad_layer)
      DrawContentQuad();
    DrawBrowserUi(render_info);
  }

  compositor_delegate_->SubmitFrame(frame_type);
}

void RenderLoop::DrawWebXr() {
  TRACE_EVENT0("gpu", __func__);
  compositor_delegate_->PrepareBufferForWebXr();

  int texture_id;
  CompositorDelegate::Transform uv_transform;
  compositor_delegate_->GetWebXrDrawParams(&texture_id, &uv_transform);
  ui_->DrawWebXr(texture_id, uv_transform);
  compositor_delegate_->OnFinishedDrawingBuffer();
}

void RenderLoop::DrawWebXrOverlay(const RenderInfo& render_info) {
  TRACE_EVENT0("gpu", __func__);
  // Calculate optimized viewport and corresponding render info.
  const auto& recommended_fovs = compositor_delegate_->GetRecommendedFovs();
  const auto& fovs = ui_->GetMinimalFovForWebXrOverlayElements(
      render_info.left_eye_model.view_matrix, recommended_fovs.first,
      render_info.right_eye_model.view_matrix, recommended_fovs.second,
      compositor_delegate_->GetZNear());
  const auto& webxr_overlay_render_info =
      compositor_delegate_->GetOptimizedRenderInfoForFovs(fovs);

  compositor_delegate_->PrepareBufferForWebXrOverlayElements();
  ui_->DrawWebVrOverlayForeground(webxr_overlay_render_info);
  compositor_delegate_->OnFinishedDrawingBuffer();
}

void RenderLoop::DrawContentQuad() {
  TRACE_EVENT0("gpu", __func__);
  compositor_delegate_->PrepareBufferForContentQuadLayer(
      ui_->GetContentWorldSpaceTransform());

  CompositorDelegate::Transform uv_transform;
  float border_x;
  float border_y;
  compositor_delegate_->GetContentQuadDrawParams(&uv_transform, &border_x,
                                                 &border_y);
  ui_->DrawContent(uv_transform, border_x, border_y);
  compositor_delegate_->OnFinishedDrawingBuffer();
}

void RenderLoop::DrawBrowserUi(const RenderInfo& render_info) {
  TRACE_EVENT0("gpu", __func__);
  compositor_delegate_->PrepareBufferForBrowserUi();
  ui_->Draw(render_info);
  compositor_delegate_->OnFinishedDrawingBuffer();
}

void RenderLoop::OnPause() {
  DCHECK(controller_delegate_);
  controller_delegate_->OnPause();
  scheduler_delegate_->OnSchedulerPause();
  ui_->OnPause();
}

void RenderLoop::OnResume() {
  DCHECK(controller_delegate_);
  scheduler_delegate_->OnSchedulerResume();
  controller_delegate_->OnResume();
}

void RenderLoop::OnSwapContents(int new_content_id) {
  ui_->OnSwapContents(new_content_id);
}

void RenderLoop::EnableAlertDialog(PlatformInputHandler* input_handler,
                                   float width,
                                   float height) {
  compositor_delegate_->SetShowingVrDialog(true);
  vr_dialog_input_delegate_ =
      std::make_unique<PlatformUiInputDelegate>(input_handler);
  vr_dialog_input_delegate_->SetSize(width, height);
  auto content_width = compositor_delegate_->GetContentBufferWidth();
  if (content_width) {
    ui_->SetContentOverlayAlertDialogEnabled(
        true, vr_dialog_input_delegate_.get(), width / content_width,
        height / content_width);
  } else {
    ui_->SetAlertDialogEnabled(true, vr_dialog_input_delegate_.get(), width,
                               height);
  }
}

void RenderLoop::DisableAlertDialog() {
  ui_->SetAlertDialogEnabled(false, nullptr, 0, 0);
  vr_dialog_input_delegate_ = nullptr;
  compositor_delegate_->SetShowingVrDialog(false);
}

void RenderLoop::SetAlertDialogSize(float width, float height) {
  if (vr_dialog_input_delegate_)
    vr_dialog_input_delegate_->SetSize(width, height);
  // If not floating, dialogs are rendered with a fixed width, so that only the
  // ratio matters. But, if they are floating, its size should be relative to
  // the contents. During a WebXR presentation, the contents are not present
  // but, in this case, the dialogs are never floating.
  auto content_width = compositor_delegate_->GetContentBufferWidth();
  if (content_width) {
    ui_->SetContentOverlayAlertDialogEnabled(
        true, vr_dialog_input_delegate_.get(), width / content_width,
        height / content_width);
  } else {
    ui_->SetAlertDialogEnabled(true, vr_dialog_input_delegate_.get(), width,
                               height);
  }
}

void RenderLoop::SetDialogLocation(float x, float y) {
  ui_->SetDialogLocation(x, y);
}

void RenderLoop::SetDialogFloating(bool floating) {
  ui_->SetDialogFloating(floating);
}

void RenderLoop::ShowToast(const base::string16& text) {
  ui_->ShowPlatformToast(text);
}

void RenderLoop::CancelToast() {
  ui_->CancelPlatformToast();
}

void RenderLoop::ContentBoundsChanged(int width, int height) {
  TRACE_EVENT0("gpu", __func__);
  ui_->OnContentBoundsChanged(width, height);
}

base::WeakPtr<BrowserUiInterface> RenderLoop::GetBrowserUiWeakPtr() {
  return ui_->GetBrowserUiWeakPtr();
}

void RenderLoop::SetUiExpectingActivityForTesting(
    UiTestActivityExpectation ui_expectation) {
  DCHECK(ui_test_state_ == nullptr)
      << "Attempted to set a UI activity expectation with one in progress";
  ui_test_state_ = std::make_unique<UiTestState>();
  ui_test_state_->quiescence_timeout_ms =
      base::TimeDelta::FromMilliseconds(ui_expectation.quiescence_timeout_ms);
}

void RenderLoop::AcceptDoffPromptForTesting() {
  ui_->AcceptDoffPromptForTesting();
}

void RenderLoop::UpdateUi(const RenderInfo& render_info,
                          base::TimeTicks current_time,
                          CompositorDelegate::FrameType frame_type) {
  TRACE_EVENT0("gpu", __func__);

  // Update the render position of all UI elements.
  base::TimeTicks timing_start = base::TimeTicks::Now();
  bool ui_updated = ui_->OnBeginFrame(current_time, render_info.head_pose);

  // WebXR handles controller input in OnVsync.
  base::TimeDelta controller_time;
  if (frame_type == CompositorDelegate::kUiFrame)
    controller_time = ProcessControllerInput(render_info, current_time);

  if (ui_->SceneHasDirtyTextures()) {
    if (!compositor_delegate_->RunInSkiaContext(base::BindOnce(
            &UiInterface::UpdateSceneTextures, base::Unretained(ui_.get())))) {
      ForceExitVr();
      return;
    }
    ui_updated = true;
  }
  ReportUiStatusForTesting(timing_start, ui_updated);

  base::TimeDelta scene_time = base::TimeTicks::Now() - timing_start;
  // Don't double-count the controller time that was part of the scene time.
  ui_processing_time_.AddSample(scene_time - controller_time);
  TRACE_EVENT_END0("gpu", "SceneUpdate");
}

device::mojom::XRInputSourceStatePtr RenderLoop::ProcessControllerInputForWebXr(
    const RenderInfo& render_info,
    base::TimeTicks current_time) {
  TRACE_EVENT0("gpu", __func__);
  DCHECK(controller_delegate_);
  DCHECK(ui_);
  base::TimeTicks timing_start = base::TimeTicks::Now();

  controller_delegate_->UpdateController(render_info, current_time, true);
  auto input_event_list = controller_delegate_->GetGestures(current_time);
  ui_->HandleMenuButtonEvents(&input_event_list);

  ui_controller_update_time_.AddSample(base::TimeTicks::Now() - timing_start);
  return controller_delegate_->GetInputSourceState();
}

base::TimeDelta RenderLoop::ProcessControllerInput(
    const RenderInfo& render_info,
    base::TimeTicks current_time) {
  TRACE_EVENT0("gpu", __func__);
  DCHECK(controller_delegate_);
  DCHECK(ui_);
  base::TimeTicks timing_start = base::TimeTicks::Now();

  controller_delegate_->UpdateController(render_info, current_time, false);
  auto input_event_list = controller_delegate_->GetGestures(current_time);
  ReticleModel reticle_model;
  ControllerModel controller_model =
      controller_delegate_->GetModel(render_info);
  ui_->HandleInput(current_time, render_info, controller_model, &reticle_model,
                   &input_event_list);
  ui_->OnControllerUpdated(controller_model, reticle_model);

  auto controller_time = base::TimeTicks::Now() - timing_start;
  ui_controller_update_time_.AddSample(controller_time);
  return controller_time;
}

void RenderLoop::ForceExitVr() {
  browser_->ForceExitVr();
}

void RenderLoop::PerformControllerActionForTesting(
    ControllerTestInput controller_input) {
  DCHECK(controller_delegate_);
  if (controller_input.action ==
      VrControllerTestAction::kRevertToRealController) {
    if (using_controller_delegate_for_testing_) {
      DCHECK(
          static_cast<ControllerDelegateForTesting*>(controller_delegate_.get())
              ->IsQueueEmpty())
          << "Attempted to revert to using real controller with actions still "
             "queued";
      using_controller_delegate_for_testing_ = false;
      controller_delegate_for_testing_.swap(controller_delegate_);
    }
    return;
  }
  if (!using_controller_delegate_for_testing_) {
    using_controller_delegate_for_testing_ = true;
    if (!controller_delegate_for_testing_)
      controller_delegate_for_testing_ =
          std::make_unique<ControllerDelegateForTesting>(ui_.get());
    controller_delegate_for_testing_.swap(controller_delegate_);
  }
  if (controller_input.action !=
      VrControllerTestAction::kEnableMockedController) {
    static_cast<ControllerDelegateForTesting*>(controller_delegate_.get())
        ->QueueControllerActionForTesting(controller_input);
  }
}

void RenderLoop::ReportUiStatusForTesting(const base::TimeTicks& current_time,
                                          bool ui_updated) {
  if (ui_test_state_ == nullptr)
    return;
  base::TimeDelta time_since_start = current_time - ui_test_state_->start_time;
  if (ui_updated) {
    ui_test_state_->activity_started = true;
    if (time_since_start > ui_test_state_->quiescence_timeout_ms) {
      // The UI is being updated, but hasn't reached a stable state in the
      // given time -> report timeout.
      ReportUiActivityResultForTesting(VrUiTestActivityResult::kTimeoutNoEnd);
    }
  } else {
    if (ui_test_state_->activity_started) {
      // The UI has been updated since the test requested notification of
      // quiescence, but wasn't this frame -> report that the UI is quiescent.
      ReportUiActivityResultForTesting(VrUiTestActivityResult::kQuiescent);
    } else if (time_since_start > ui_test_state_->quiescence_timeout_ms) {
      // The UI has never been updated and we've reached the timeout.
      ReportUiActivityResultForTesting(VrUiTestActivityResult::kTimeoutNoStart);
    }
  }
}

void RenderLoop::ReportUiActivityResultForTesting(
    VrUiTestActivityResult result) {
  ui_test_state_ = nullptr;
  browser_->ReportUiActivityResultForTesting(result);
}

}  // namespace vr
