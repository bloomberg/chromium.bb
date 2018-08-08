// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/controller_delegate_for_testing.h"

#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_interface.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/ui_test_input.h"

namespace {

// Laser origin relative to the center of the controller.
constexpr gfx::Point3F kLaserOriginOffset = {0.0f, 0.0f, -0.05f};

// We position the controller in a fixed position (no arm model).
// The location constants are approximations that allow us to have the
// controller and the laser visible on the screenshots.
void SetOriginAndTransform(vr::ControllerModel* model) {
  gfx::Transform mat;
  mat.Translate3d(vr::kStartControllerPosition);
  mat.PreconcatTransform(gfx::Transform(
      gfx::Quaternion(vr::kForwardVector, model->laser_direction)));
  model->transform = mat;
  model->laser_origin = kLaserOriginOffset;
  mat.TransformPoint(&model->laser_origin);
}

}  // namespace

namespace vr {

ControllerDelegateForTesting::ControllerDelegateForTesting(UiInterface* ui)
    : ui_(ui) {
  cached_controller_model_.laser_direction = kForwardVector;
  SetOriginAndTransform(&cached_controller_model_);
}

ControllerDelegateForTesting::~ControllerDelegateForTesting() = default;

void ControllerDelegateForTesting::QueueControllerActionForTesting(
    ControllerTestInput controller_input) {
  DCHECK_NE(controller_input.action,
            VrControllerTestAction::kRevertToRealController);
  ControllerModel controller_model;
  auto target_point = ui_->GetTargetPointForTesting(
      controller_input.element_name, controller_input.position);
  auto direction = (target_point - kStartControllerPosition) - kOrigin;
  direction.GetNormalized(&controller_model.laser_direction);
  SetOriginAndTransform(&controller_model);

  switch (controller_input.action) {
    case VrControllerTestAction::kClick:
      // Add in the button down action.
      controller_model.touchpad_button_state =
          UiInputManager::ButtonState::DOWN;
      controller_model_queue_.push(controller_model);
      // Add in the button up action.
      controller_model.touchpad_button_state = UiInputManager::ButtonState::UP;
      controller_model_queue_.push(controller_model);
      break;
    case VrControllerTestAction::kHover:
      FALLTHROUGH;
    case VrControllerTestAction::kClickUp:
      controller_model.touchpad_button_state = UiInputManager::ButtonState::UP;
      controller_model_queue_.push(controller_model);
      break;
    case VrControllerTestAction::kClickDown:
      controller_model.touchpad_button_state =
          UiInputManager::ButtonState::DOWN;
      controller_model_queue_.push(controller_model);
      break;
    case VrControllerTestAction::kMove:
      // Use whatever the last button state is.
      if (!IsQueueEmpty()) {
        controller_model.touchpad_button_state =
            controller_model_queue_.back().touchpad_button_state;
      } else {
        controller_model.touchpad_button_state =
            cached_controller_model_.touchpad_button_state;
      }
      controller_model_queue_.push(controller_model);
      break;
    default:
      NOTREACHED() << "Given unsupported controller action";
  }
}

bool ControllerDelegateForTesting::IsQueueEmpty() const {
  return controller_model_queue_.empty();
}

void ControllerDelegateForTesting::UpdateController(
    const RenderInfo& render_info,
    base::TimeTicks current_time,
    bool is_webxr_frame) {
  if (!controller_model_queue_.empty()) {
    cached_controller_model_ = controller_model_queue_.front();
    controller_model_queue_.pop();
  }
  cached_controller_model_.last_orientation_timestamp = current_time;
  cached_controller_model_.last_button_timestamp = current_time;
}

ControllerModel ControllerDelegateForTesting::GetModel(
    const RenderInfo& render_info) {
  return cached_controller_model_;
}

InputEventList ControllerDelegateForTesting::GetGestures(
    base::TimeTicks current_time) {
  return InputEventList();
}

device::mojom::XRInputSourceStatePtr
ControllerDelegateForTesting::GetInputSourceState() {
  auto state = device::mojom::XRInputSourceState::New();
  state->description = device::mojom::XRInputSourceDescription::New();
  state->source_id = 1;
  state->description->target_ray_mode =
      device::mojom::XRTargetRayMode::POINTING;
  state->description->emulated_position = true;

  return state;
}

void ControllerDelegateForTesting::OnResume() {}

void ControllerDelegateForTesting::OnPause() {}

}  // namespace vr
