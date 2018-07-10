// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_controller.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/numerics/math_constants.h"
#include "base/numerics/ranges.h"
#include "chrome/browser/vr/input_event.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_controller.h"

namespace vr {

namespace {

constexpr float kNanoSecondsPerSecond = 1.0e9f;

// Distance from the center of the controller to start rendering the laser.
constexpr float kLaserStartDisplacement = 0.045;

constexpr float kFadeDistanceFromFace = 0.34f;
constexpr float kDeltaAlpha = 3.0f;

// Small deadzone for testing that prevents the controller's head offset from
// being updated every frame on 3DOF devices.
constexpr float kHeadOffsetDeadzone = 0.0005f;

void ClampTouchpadPosition(gfx::Vector2dF* position) {
  position->set_x(base::ClampToRange(position->x(), 0.0f, 1.0f));
  position->set_y(base::ClampToRange(position->y(), 0.0f, 1.0f));
}

float DeltaTimeSeconds(int64_t last_timestamp_nanos) {
  return (gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos -
          last_timestamp_nanos) /
         kNanoSecondsPerSecond;
}

gvr::ControllerButton PlatformToGvrButton(PlatformController::ButtonType type) {
  switch (type) {
    case PlatformController::kButtonHome:
      return gvr::kControllerButtonHome;
    case PlatformController::kButtonMenu:
      return gvr::kControllerButtonApp;
    case PlatformController::kButtonSelect:
      return gvr::kControllerButtonClick;
    default:
      return gvr::kControllerButtonNone;
  }
}

}  // namespace

VrController::VrController(gvr_context* gvr_context)
    : previous_button_states_{0} {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  CHECK(gvr_context != nullptr) << "invalid gvr_context";
  controller_api_ = std::make_unique<gvr::ControllerApi>();
  controller_state_ = std::make_unique<gvr::ControllerState>();
  gvr_api_ = gvr::GvrApi::WrapNonOwned(gvr_context);

  int32_t options = gvr::ControllerApi::DefaultOptions();

  options |= GVR_CONTROLLER_ENABLE_ARM_MODEL;

  // Enable non-default options - WebVR needs gyro and linear acceleration, and
  // since VrShell implements GvrGamepadDataProvider we need this always.
  options |= GVR_CONTROLLER_ENABLE_GYRO;
  options |= GVR_CONTROLLER_ENABLE_ACCEL;

  CHECK(controller_api_->Init(options, gvr_context));
  controller_api_->Resume();

  handedness_ = gvr_api_->GetUserPrefs().GetControllerHandedness();

  gesture_detector_ = std::make_unique<GestureDetector>();
  last_timestamp_nanos_ =
      gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos;
}

VrController::~VrController() {
  DVLOG(1) << __FUNCTION__ << "=" << this;
}

void VrController::OnResume() {
  if (controller_api_) {
    controller_api_->Resume();
    handedness_ = gvr_api_->GetUserPrefs().GetControllerHandedness();
  }
}

void VrController::OnPause() {
  if (controller_api_)
    controller_api_->Pause();
}

device::GvrGamepadData VrController::GetGamepadData() {
  device::GvrGamepadData pad = {};
  pad.connected = IsConnected();
  pad.timestamp = controller_state_->GetLastOrientationTimestamp();

  if (pad.connected) {
    pad.touch_pos.set_x(TouchPosX());
    pad.touch_pos.set_y(TouchPosY());
    pad.orientation = Orientation();

    // Use orientation to rotate acceleration/gyro into seated space.
    gfx::Transform pose_mat(Orientation());
    const gvr::Vec3f& accel = controller_state_->GetAccel();
    const gvr::Vec3f& gyro = controller_state_->GetGyro();
    pad.accel = gfx::Vector3dF(accel.x, accel.y, accel.z);
    pose_mat.TransformVector(&pad.accel);
    pad.gyro = gfx::Vector3dF(gyro.x, gyro.y, gyro.z);
    pose_mat.TransformVector(&pad.gyro);

    pad.is_touching = controller_state_->IsTouching();
    pad.controller_button_pressed =
        controller_state_->GetButtonState(GVR_CONTROLLER_BUTTON_CLICK);
    pad.right_handed = handedness_ == GVR_CONTROLLER_RIGHT_HANDED;
  }

  return pad;
}

device::mojom::XRInputSourceStatePtr VrController::GetInputSourceState() {
  device::mojom::XRInputSourceStatePtr state =
      device::mojom::XRInputSourceState::New();

  // Only one controller is supported, so the source id can be static.
  state->source_id = 1;

  // Set the primary button state.
  state->primary_input_pressed = ButtonState(GVR_CONTROLLER_BUTTON_CLICK);

  if (ButtonUpHappened(GVR_CONTROLLER_BUTTON_CLICK))
    state->primary_input_clicked = true;

  state->description = device::mojom::XRInputSourceDescription::New();

  // It's a handheld pointing device.
  state->description->target_ray_mode =
      device::mojom::XRTargetRayMode::POINTING;

  // Controller uses an arm model.
  state->description->emulated_position = true;

  // Set handedness.
  switch (handedness_) {
    case GVR_CONTROLLER_LEFT_HANDED:
      state->description->handedness = device::mojom::XRHandedness::LEFT;
      break;
    case GVR_CONTROLLER_RIGHT_HANDED:
      state->description->handedness = device::mojom::XRHandedness::RIGHT;
      break;
    default:
      state->description->handedness = device::mojom::XRHandedness::NONE;
      break;
  }

  // Get the grip transform
  gfx::Transform grip;
  GetTransform(&grip);
  state->grip = grip;

  // Set the pointer offset from the grip transform.
  gfx::Transform pointer;
  GetRelativePointerTransform(&pointer);
  state->description->pointer_offset = pointer;

  return state;
}

bool VrController::IsTouching() {
  return controller_state_->IsTouching();
}

float VrController::TouchPosX() {
  return controller_state_->GetTouchPos().x;
}

float VrController::TouchPosY() {
  return controller_state_->GetTouchPos().y;
}

bool VrController::IsButtonDown(PlatformController::ButtonType type) const {
  return controller_state_->GetButtonState(PlatformToGvrButton(type));
}

base::TimeTicks VrController::GetLastOrientationTimestamp() const {
  // controller_state_->GetLast*Timestamp() returns timestamps in a
  // different timebase from base::TimeTicks::Now(), so we can't use the
  // timestamps in any meaningful way in the rest of Chrome.
  // TODO(mthiesse): Use controller_state_->GetLastOrientationTimestamp() when
  // b/62818778 is resolved.
  return base::TimeTicks::Now();
}

base::TimeTicks VrController::GetLastTouchTimestamp() const {
  // TODO(mthiesse): Use controller_state_->GetLastTouchTimestamp() when
  // b/62818778 is resolved.
  return base::TimeTicks::Now();
}

base::TimeTicks VrController::GetLastButtonTimestamp() const {
  // TODO(mthiesse): Use controller_state_->GetLastButtonTimestamp() when
  // b/62818778 is resolved.
  return base::TimeTicks::Now();
}

PlatformController::Handedness VrController::GetHandedness() const {
  return handedness_ == GVR_CONTROLLER_RIGHT_HANDED
             ? PlatformController::kRightHanded
             : PlatformController::kLeftHanded;
}

bool VrController::GetRecentered() const {
  return controller_state_->GetRecentered();
}

int VrController::GetBatteryLevel() const {
  return static_cast<int>(controller_state_->GetBatteryLevel());
}

gfx::Quaternion VrController::Orientation() const {
  const gvr::Quatf& orientation = controller_state_->GetOrientation();
  return gfx::Quaternion(orientation.qx, orientation.qy, orientation.qz,
                         orientation.qw);
}

gfx::Point3F VrController::Position() const {
  const gvr::Vec3f& position = controller_state_->GetPosition();
  return gfx::Point3F(position.x + head_offset_.x(),
                      position.y + head_offset_.y(),
                      position.z + head_offset_.z());
}

void VrController::GetTransform(gfx::Transform* out) const {
  *out = gfx::Transform(Orientation());
  const gfx::Point3F& position = Position();
  out->matrix().postTranslate(position.x(), position.y(), position.z());
}

void VrController::GetRelativePointerTransform(gfx::Transform* out) const {
  *out = gfx::Transform();
  out->RotateAboutXAxis(-kErgoAngleOffset * 180.0f / base::kPiFloat);
  out->Translate3d(0, 0, -kLaserStartDisplacement);
}

void VrController::GetPointerTransform(gfx::Transform* out) const {
  gfx::Transform controller;
  GetTransform(&controller);

  GetRelativePointerTransform(out);
  out->ConcatTransform(controller);
}

float VrController::GetOpacity() const {
  return alpha_value_;
}

gfx::Point3F VrController::GetPointerStart() const {
  gfx::Transform pointer_transform;
  GetPointerTransform(&pointer_transform);

  gfx::Point3F pointer_position;
  pointer_transform.TransformPoint(&pointer_position);
  return pointer_position;
}

bool VrController::TouchDownHappened() {
  return controller_state_->GetTouchDown();
}

bool VrController::TouchUpHappened() {
  return controller_state_->GetTouchUp();
}

bool VrController::ButtonDownHappened(gvr::ControllerButton button) {
  // Workaround for GVR sometimes not reporting GetButtonDown when it should.
  bool detected_down =
      !previous_button_states_[static_cast<int>(button)] && ButtonState(button);
  return controller_state_->GetButtonDown(button) || detected_down;
}

bool VrController::ButtonUpHappened(gvr::ControllerButton button) {
  // Workaround for GVR sometimes not reporting GetButtonUp when it should.
  bool detected_up =
      previous_button_states_[static_cast<int>(button)] && !ButtonState(button);
  return controller_state_->GetButtonUp(button) || detected_up;
}

bool VrController::ButtonState(gvr::ControllerButton button) const {
  return controller_state_->GetButtonState(button);
}

bool VrController::IsConnected() {
  return controller_state_->GetConnectionState() == gvr::kControllerConnected;
}

void VrController::EnableDeadzoneForTesting() {
  enable_deadzone_ = true;
}

void VrController::UpdateState(const gfx::Transform& head_pose) {
  gfx::Transform inv_pose;
  if (head_pose.GetInverse(&inv_pose)) {
    auto current_head_offset = gfx::Point3F();
    inv_pose.TransformPoint(&current_head_offset);
    // TODO(https://crbug.com/861807): Remove this once the controller can be
    // dirty without necessarily affecting quiescence.
    if (enable_deadzone_) {
      if (head_offset_.SquaredDistanceTo(current_head_offset) >
          kHeadOffsetDeadzone) {
        head_offset_ = current_head_offset;
      }
    } else {
      head_offset_ = current_head_offset;
    }
  }

  gvr::Mat4f gvr_head_pose;
  TransformToGvrMat(head_pose, &gvr_head_pose);
  controller_api_->ApplyArmModel(handedness_, gvr::kArmModelBehaviorFollowGaze,
                                 gvr_head_pose);
  const int32_t old_status = controller_state_->GetApiStatus();
  const int32_t old_connection_state = controller_state_->GetConnectionState();
  for (int button = 0; button < GVR_CONTROLLER_BUTTON_COUNT; ++button) {
    previous_button_states_[button] =
        ButtonState(static_cast<gvr_controller_button>(button));
  }
  // Read current controller state.
  controller_state_->Update(*controller_api_);
  // Print new API status and connection state, if they changed.
  if (controller_state_->GetApiStatus() != old_status ||
      controller_state_->GetConnectionState() != old_connection_state) {
    VLOG(1) << "Controller Connection status: "
            << gvr_controller_connection_state_to_string(
                   controller_state_->GetConnectionState());
  }
  UpdateAlpha();
  last_timestamp_nanos_ =
      gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos;
}

std::unique_ptr<InputEventList> VrController::DetectGestures() {
  if (controller_state_->GetConnectionState() != gvr::kControllerConnected) {
    return std::make_unique<InputEventList>();
  }

  UpdateCurrentTouchInfo();
  return gesture_detector_->DetectGestures(
      touch_info_, base::TimeTicks::Now(),
      ButtonState(gvr::kControllerButtonClick));
}

void VrController::UpdateCurrentTouchInfo() {
  touch_info_.touch_up = TouchUpHappened();
  touch_info_.touch_down = TouchDownHappened();
  touch_info_.is_touching = IsTouching();
  touch_info_.touch_point.position.set_x(TouchPosX());
  touch_info_.touch_point.position.set_y(TouchPosY());
  ClampTouchpadPosition(&touch_info_.touch_point.position);
  if (touch_info_.is_touching) {
    touch_info_.touch_point.timestamp =
        base::TimeTicks() +
        base::TimeDelta::FromNanoseconds(
            gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos);
  }

}

void VrController::UpdateAlpha() {
  float distance_to_face = (Position() - gfx::Point3F()).Length();
  float alpha_change = kDeltaAlpha * DeltaTimeSeconds(last_timestamp_nanos_);
  alpha_value_ = base::ClampToRange(distance_to_face < kFadeDistanceFromFace
                                        ? alpha_value_ - alpha_change
                                        : alpha_value_ + alpha_change,
                                    0.0f, 1.0f);
}

}  // namespace vr
