// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/render_loop.h"

#include <utility>

#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "chrome/browser/vr/controller_delegate.h"
#include "chrome/browser/vr/graphics_delegate.h"
#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/render_loop_browser_interface.h"
#include "chrome/browser/vr/ui_interface.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_test_input.h"

namespace vr {

RenderLoop::RenderLoop(std::unique_ptr<UiInterface> ui,
                       RenderLoopBrowserInterface* browser,
                       size_t sliding_time_size)
    : ui_(std::move(ui)),
      browser_(browser),
      ui_processing_time_(sliding_time_size),
      ui_controller_update_time_(sliding_time_size) {}
RenderLoop::~RenderLoop() = default;

void RenderLoop::SetUiExpectingActivityForTesting(
    UiTestActivityExpectation ui_expectation) {
  DCHECK(ui_test_state_ == nullptr)
      << "Attempted to set a UI activity expectation with one in progress";
  ui_test_state_ = std::make_unique<UiTestState>();
  ui_test_state_->quiescence_timeout_ms =
      base::TimeDelta::FromMilliseconds(ui_expectation.quiescence_timeout_ms);
}

void RenderLoop::UpdateUi(const RenderInfo& render_info,
                          base::TimeTicks current_time,
                          FrameType frame_type) {
  TRACE_EVENT0("gpu", __func__);
  DCHECK(graphics_delegate_);

  // Update the render position of all UI elements.
  base::TimeTicks timing_start = base::TimeTicks::Now();
  bool ui_updated = ui_->OnBeginFrame(current_time, render_info.head_pose);

  // WebXR handles controller input in OnVsync.
  base::TimeDelta controller_time;
  if (frame_type == kUiFrame)
    controller_time =
        ProcessControllerInput(render_info, current_time, kUiFrame);

  if (ui_->SceneHasDirtyTextures()) {
    if (!graphics_delegate_->MakeSkiaContextCurrent()) {
      ForceExitVr();
      return;
    }
    ui_->UpdateSceneTextures();
    if (!graphics_delegate_->MakeMainContextCurrent()) {
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

base::TimeDelta RenderLoop::ProcessControllerInput(
    const RenderInfo& render_info,
    base::TimeTicks current_time,
    FrameType frame_type) {
  TRACE_EVENT0("gpu", __func__);
  DCHECK(controller_delegate_);
  DCHECK(ui_);
  base::TimeTicks timing_start = base::TimeTicks::Now();

  controller_delegate_->UpdateController(render_info, current_time, frame_type);
  auto input_event_list = controller_delegate_->GetGestures(current_time);
  if (frame_type == kWebXrFrame) {
    ui_->HandleMenuButtonEvents(&input_event_list);
  } else {
    ReticleModel reticle_model;
    ControllerModel controller_model =
        controller_delegate_->GetModel(render_info);
    ui_->HandleInput(current_time, render_info, controller_model,
                     &reticle_model, &input_event_list);
    ui_->OnControllerUpdated(controller_model, reticle_model);
  }

  auto controller_time = base::TimeTicks::Now() - timing_start;
  ui_controller_update_time_.AddSample(controller_time);
  return controller_time;
}

void RenderLoop::ForceExitVr() {
  browser_->ForceExitVr();
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
