// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/gvr_controller_delegate.h"

#include <utility>

#include "chrome/browser/android/vr/gl_browser_interface.h"
#include "chrome/browser/android/vr/vr_controller.h"
#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/pose_util.h"
#include "chrome/browser/vr/render_info.h"

namespace {
constexpr gfx::Vector3dF kForwardVector = {0.0f, 0.0f, -1.0f};
}

namespace vr {

GvrControllerDelegate::GvrControllerDelegate(gvr::GvrApi* gvr_api,
                                             GlBrowserInterface* browser)
    : controller_(std::make_unique<VrController>(gvr_api)), browser_(browser) {}

GvrControllerDelegate::~GvrControllerDelegate() = default;

void GvrControllerDelegate::UpdateController(const RenderInfo& render_info,
                                             base::TimeTicks current_time,
                                             bool is_webxr_frame) {
  controller_->UpdateState(render_info.head_pose);

  device::GvrGamepadData controller_data = controller_->GetGamepadData();
  if (!is_webxr_frame)
    controller_data.connected = false;
  browser_->UpdateGamepadData(controller_data);
}

ControllerModel GvrControllerDelegate::GetModel(const RenderInfo& render_info) {
  gfx::Vector3dF head_direction = GetForwardVector(render_info.head_pose);

  gfx::Vector3dF controller_direction;
  gfx::Quaternion controller_quat;
  if (!controller_->IsConnected()) {
    // No controller detected, set up a gaze cursor that tracks the forward
    // direction.
    controller_direction = kForwardVector;
    controller_quat = gfx::Quaternion(kForwardVector, head_direction);
  } else {
    controller_direction = {0.0f, -sin(kErgoAngleOffset),
                            -cos(kErgoAngleOffset)};
    controller_quat = controller_->Orientation();
  }
  gfx::Transform(controller_quat).TransformVector(&controller_direction);

  ControllerModel controller_model;
  controller_->GetTransform(&controller_model.transform);
  controller_model.touchpad_button_state = PlatformController::ButtonState::kUp;
  DCHECK(!(controller_->ButtonUpHappened(PlatformController::kButtonSelect) &&
           controller_->ButtonDownHappened(PlatformController::kButtonSelect)))
      << "Cannot handle a button down and up event within one frame.";
  if (controller_->ButtonState(gvr::kControllerButtonClick)) {
    controller_model.touchpad_button_state =
        PlatformController::ButtonState::kDown;
  }
  controller_model.app_button_state =
      controller_->ButtonState(gvr::kControllerButtonApp)
          ? PlatformController::ButtonState::kDown
          : PlatformController::ButtonState::kUp;
  controller_model.home_button_state =
      controller_->ButtonState(gvr::kControllerButtonHome)
          ? PlatformController::ButtonState::kDown
          : PlatformController::ButtonState::kUp;
  controller_model.opacity = controller_->GetOpacity();
  controller_model.laser_direction = controller_direction;
  controller_model.laser_origin = controller_->GetPointerStart();
  controller_model.handedness = controller_->GetHandedness();
  controller_model.recentered = controller_->GetRecentered();
  controller_model.touching_touchpad = controller_->IsTouchingTrackpad();
  controller_model.touchpad_touch_position =
      controller_->GetPositionInTrackpad();
  controller_model.last_orientation_timestamp =
      controller_->GetLastOrientationTimestamp();
  controller_model.last_button_timestamp =
      controller_->GetLastButtonTimestamp();
  controller_model.battery_level = controller_->GetBatteryLevel();
  return controller_model;
}

InputEventList GvrControllerDelegate::GetGestures(
    base::TimeTicks current_time) {
  if (!controller_->IsConnected())
    return {};
  return gesture_detector_.DetectGestures(*controller_, current_time);
}

device::mojom::XRInputSourceStatePtr
GvrControllerDelegate::GetInputSourceState() {
  device::mojom::XRInputSourceStatePtr state =
      device::mojom::XRInputSourceState::New();
  state->description = device::mojom::XRInputSourceDescription::New();

  // Only one controller is supported, so the source id can be static.
  state->source_id = 1;

  // It's a handheld pointing device.
  state->description->target_ray_mode =
      device::mojom::XRTargetRayMode::POINTING;

  // Controller uses an arm model.
  state->description->emulated_position = true;

  if (controller_->IsConnected()) {
    // Set the primary button state.
    bool select_button_down =
        controller_->IsButtonDown(PlatformController::kButtonSelect);
    state->primary_input_pressed = select_button_down;
    state->primary_input_clicked =
        was_select_button_down_ && !select_button_down;
    was_select_button_down_ = select_button_down;

    // Set handedness.
    state->description->handedness =
        controller_->GetHandedness() == PlatformController::kRightHanded
            ? device::mojom::XRHandedness::RIGHT
            : device::mojom::XRHandedness::LEFT;

    // Get the grip transform
    gfx::Transform grip;
    controller_->GetTransform(&grip);
    state->grip = grip;

    // Set the pointer offset from the grip transform.
    gfx::Transform pointer;
    controller_->GetRelativePointerTransform(&pointer);
    state->description->pointer_offset = pointer;
  }

  return state;
}

void GvrControllerDelegate::OnResume() {
  controller_->OnResume();
}

void GvrControllerDelegate::OnPause() {
  controller_->OnPause();
}

}  // namespace vr
