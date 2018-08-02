// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/render_loop.h"

#include <utility>

#include "base/time/time.h"
#include "chrome/browser/vr/controller_delegate.h"
#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/ui_interface.h"

namespace vr {

RenderLoop::RenderLoop(std::unique_ptr<UiInterface> ui) : ui_(std::move(ui)) {}
RenderLoop::~RenderLoop() = default;

void RenderLoop::ProcessControllerInput(const RenderInfo& render_info,
                                        base::TimeTicks current_time,
                                        bool is_webxr_frame) {
  DCHECK(controller_delegate_);
  DCHECK(ui_);

  controller_delegate_->UpdateController(render_info, current_time,
                                         is_webxr_frame);
  auto input_event_list = controller_delegate_->GetGestures(current_time);
  if (is_webxr_frame) {
    ui_->HandleMenuButtonEvents(&input_event_list);
  } else {
    ReticleModel reticle_model;
    ControllerModel controller_model =
        controller_delegate_->GetModel(render_info);
    ui_->HandleInput(current_time, render_info, controller_model,
                     &reticle_model, &input_event_list);
    ui_->OnControllerUpdated(controller_model, reticle_model);
  }
}

}  // namespace vr
