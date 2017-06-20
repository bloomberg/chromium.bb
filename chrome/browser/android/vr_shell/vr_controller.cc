// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_controller.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cc/base/math_util.h"
#include "chrome/browser/android/vr_shell/elbow_model.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_controller.h"
#include "ui/gfx/transform.h"

namespace vr_shell {

namespace {

constexpr float kDisplacementScaleFactor = 300.0f;

// A slop represents a small rectangular region around the first touch point of
// a gesture.
// If the user does not move outside of the slop, no gesture is detected.
// Gestures start to be detected when the user moves outside of the slop.
// Vertical distance from the border to the center of slop.
constexpr float kSlopVertical = 0.165f;

// Horizontal distance from the border to the center of slop.
constexpr float kSlopHorizontal = 0.15f;

// Minimum distance needed in at least one direction to call two vectors
// not equal. Also, minimum time distance needed to call two timestamps
// not equal.
constexpr float kDelta = 1.0e-7f;

constexpr float kCutoffHz = 10.0f;
constexpr float kRC = static_cast<float>(1.0 / (2.0 * M_PI * kCutoffHz));
constexpr float kNanoSecondsPerSecond = 1.0e9f;

constexpr int kMaxNumOfExtrapolations = 2;

// Distance from the center of the controller to start rendering the laser.
constexpr float kLaserStartDisplacement = 0.045;

constexpr int microsPerNano = 1000;

void ClampTouchpadPosition(gfx::Vector2dF* position) {
  position->set_x(cc::MathUtil::ClampToRange(position->x(), 0.0f, 1.0f));
  position->set_y(cc::MathUtil::ClampToRange(position->y(), 0.0f, 1.0f));
}

float DeltaTimeSeconds(int64_t last_timestamp_nanos) {
  return (gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos -
          last_timestamp_nanos) /
         kNanoSecondsPerSecond;
}

}  // namespace

VrController::VrController(gvr_context* gvr_context) {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  CHECK(gvr_context != nullptr) << "invalid gvr_context";
  controller_api_ = base::MakeUnique<gvr::ControllerApi>();
  controller_state_ = base::MakeUnique<gvr::ControllerState>();
  gvr_api_ = gvr::GvrApi::WrapNonOwned(gvr_context);

  int32_t options = gvr::ControllerApi::DefaultOptions();

  // Enable non-default options - WebVR needs gyro and linear acceleration, and
  // since VrShell implements GvrGamepadDataProvider we need this always.
  options |= GVR_CONTROLLER_ENABLE_GYRO;
  options |= GVR_CONTROLLER_ENABLE_ACCEL;

  CHECK(controller_api_->Init(options, gvr_context));
  controller_api_->Resume();

  handedness_ = gvr_api_->GetUserPrefs().GetControllerHandedness();
  elbow_model_ = base::MakeUnique<ElbowModel>(handedness_);

  Reset();
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
    elbow_model_->SetHandedness(handedness_);
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

bool VrController::IsTouching() {
  return controller_state_->IsTouching();
}

float VrController::TouchPosX() {
  return controller_state_->GetTouchPos().x;
}

float VrController::TouchPosY() {
  return controller_state_->GetTouchPos().y;
}

base::TimeTicks VrController::GetLastOrientationTimestamp() const {
  return base::TimeTicks::FromInternalValue(
      controller_state_->GetLastOrientationTimestamp() / microsPerNano);
}

base::TimeTicks VrController::GetLastTouchTimestamp() const {
  return base::TimeTicks::FromInternalValue(
      controller_state_->GetLastTouchTimestamp() / microsPerNano);
}

base::TimeTicks VrController::GetLastButtonTimestamp() const {
  return base::TimeTicks::FromInternalValue(
      controller_state_->GetLastButtonTimestamp() / microsPerNano);
}

gfx::Quaternion VrController::Orientation() const {
  const gvr::Quatf& orientation = controller_state_->GetOrientation();
  return gfx::Quaternion(orientation.qx, orientation.qy, orientation.qz,
                         orientation.qw);
}

void VrController::GetTransform(gfx::Transform* out) const {
  *out = gfx::Transform(elbow_model_->GetControllerRotation());
  gfx::Point3F p = elbow_model_->GetControllerPosition();
  out->matrix().postTranslate(p.x(), p.y(), p.z());
}

float VrController::GetOpacity() const {
  return elbow_model_->GetAlphaValue();
}

gfx::Point3F VrController::GetPointerStart() const {
  gfx::Point3F controller_position = elbow_model_->GetControllerPosition();
  gfx::Vector3dF pointer_direction{0.0f, -sin(kErgoAngleOffset),
                                   -cos(kErgoAngleOffset)};
  gfx::Transform rotation_mat(Orientation());
  rotation_mat.TransformVector(&pointer_direction);
  return controller_position +
         gfx::ScaleVector3d(pointer_direction, kLaserStartDisplacement);
}

VrControllerModel::State VrController::GetModelState() const {
  if (ButtonState(gvr::ControllerButton::GVR_CONTROLLER_BUTTON_CLICK))
    return VrControllerModel::TOUCHPAD;
  if (ButtonState(gvr::ControllerButton::GVR_CONTROLLER_BUTTON_APP))
    return VrControllerModel::APP;
  if (ButtonState(gvr::ControllerButton::GVR_CONTROLLER_BUTTON_HOME))
    return VrControllerModel::SYSTEM;
  return VrControllerModel::IDLE;
}

bool VrController::TouchDownHappened() {
  return controller_state_->GetTouchDown();
}

bool VrController::TouchUpHappened() {
  return controller_state_->GetTouchUp();
}

bool VrController::ButtonDownHappened(gvr::ControllerButton button) {
  return controller_state_->GetButtonDown(button);
}

bool VrController::ButtonUpHappened(gvr::ControllerButton button) {
  return controller_state_->GetButtonUp(button);
}

bool VrController::ButtonState(gvr::ControllerButton button) const {
  return controller_state_->GetButtonState(button);
}

bool VrController::IsConnected() {
  return controller_state_->GetConnectionState() == gvr::kControllerConnected;
}

void VrController::UpdateState(const gfx::Vector3dF& head_direction) {
  const int32_t old_status = controller_state_->GetApiStatus();
  const int32_t old_connection_state = controller_state_->GetConnectionState();
  // Read current controller state.
  controller_state_->Update(*controller_api_);
  // Print new API status and connection state, if they changed.
  if (controller_state_->GetApiStatus() != old_status ||
      controller_state_->GetConnectionState() != old_connection_state) {
    VLOG(1) << "Controller Connection status: "
            << gvr_controller_connection_state_to_string(
                   controller_state_->GetConnectionState());
  }

  const gvr::Vec3f& gvr_gyro = controller_state_->GetGyro();
  elbow_model_->Update({IsConnected(), Orientation(),
                        gfx::Vector3dF(gvr_gyro.x, gvr_gyro.y, gvr_gyro.z),
                        head_direction,
                        DeltaTimeSeconds(last_timestamp_nanos_)});
}

void VrController::UpdateTouchInfo() {
  CHECK(touch_info_ != nullptr) << "touch_info_ not initialized properly.";
  if (IsTouching() && state_ == SCROLLING &&
      (controller_state_->GetLastTouchTimestamp() == last_touch_timestamp_ ||
       (cur_touch_point_->position == prev_touch_point_->position &&
        extrapolated_touch_ < kMaxNumOfExtrapolations))) {
    extrapolated_touch_++;
    touch_position_changed_ = true;
    // Fill the touch_info
    float duration = DeltaTimeSeconds(last_timestamp_nanos_);
    touch_info_->touch_point.position.set_x(cur_touch_point_->position.x() +
                                            overall_velocity_.x() * duration);
    touch_info_->touch_point.position.set_y(cur_touch_point_->position.y() +
                                            overall_velocity_.y() * duration);
  } else {
    if (extrapolated_touch_ == kMaxNumOfExtrapolations) {
      overall_velocity_ = {0, 0};
    }
    extrapolated_touch_ = 0;
  }
  last_touch_timestamp_ = controller_state_->GetLastTouchTimestamp();
  last_timestamp_nanos_ =
      gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos;
}

std::unique_ptr<GestureList> VrController::DetectGestures() {
  std::unique_ptr<GestureList> gesture_list = base::MakeUnique<GestureList>();
  std::unique_ptr<blink::WebGestureEvent> gesture(new blink::WebGestureEvent());

  if (controller_state_->GetConnectionState() != gvr::kControllerConnected) {
    gesture_list->push_back(std::move(gesture));
    return gesture_list;
  }

  touch_position_changed_ = UpdateCurrentTouchpoint();
  UpdateTouchInfo();
  if (touch_position_changed_)
    UpdateOverallVelocity();

  UpdateGestureFromTouchInfo(gesture.get());
  gesture->source_device = blink::kWebGestureDeviceTouchpad;
  gesture_list->push_back(std::move(gesture));

  if (gesture_list->back()->GetType() ==
      blink::WebInputEvent::kGestureScrollEnd) {
    if (!ButtonDownHappened(gvr::kControllerButtonClick) &&
        (last_velocity_.x() != 0.0 || last_velocity_.y() != 0.0)) {
      std::unique_ptr<blink::WebGestureEvent> fling(
          new blink::WebGestureEvent(blink::WebInputEvent::kGestureFlingStart,
                                     blink::WebInputEvent::kNoModifiers,
                                     gesture_list->back()->TimeStampSeconds()));
      fling->source_device = blink::kWebGestureDeviceTouchpad;
      if (IsHorizontalGesture()) {
        fling->data.fling_start.velocity_x =
            last_velocity_.x() * kDisplacementScaleFactor;
      } else {
        fling->data.fling_start.velocity_y =
            last_velocity_.y() * kDisplacementScaleFactor;
      }
      gesture_list->push_back(std::move(fling));
    }
    Reset();
  }

  return gesture_list;
}

void VrController::UpdateGestureFromTouchInfo(blink::WebGestureEvent* gesture) {
  gesture->SetTimeStampSeconds(
      (GetLastTouchTimestamp() - base::TimeTicks()).InSecondsF());
  switch (state_) {
    // User has not put finger on touch pad.
    case WAITING:
      HandleWaitingState(gesture);
      break;
    // User has not started a gesture (by moving out of slop).
    case TOUCHING:
      HandleDetectingState(gesture);
      break;
    // User is scrolling on touchpad
    case SCROLLING:
      HandleScrollingState(gesture);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void VrController::HandleWaitingState(blink::WebGestureEvent* gesture) {
  // User puts finger on touch pad (or when the touch down for current gesture
  // is missed, initiate gesture from current touch point).
  if (touch_info_->touch_down || touch_info_->is_touching) {
    // update initial touchpoint
    *init_touch_point_ = touch_info_->touch_point;
    // update current touchpoint
    *cur_touch_point_ = touch_info_->touch_point;
    state_ = TOUCHING;

    gesture->SetType(blink::WebInputEvent::kGestureFlingCancel);
    gesture->data.fling_cancel.prevent_boosting = false;
  }
}

void VrController::HandleDetectingState(blink::WebGestureEvent* gesture) {
  // User lifts up finger from touch pad.
  if (touch_info_->touch_up || !(touch_info_->is_touching)) {
    Reset();
    return;
  }

  // Touch position is changed, the touch point moves outside of slop,
  // and the Controller's button is not down.
  if (touch_position_changed_ && touch_info_->is_touching &&
      !InSlop(touch_info_->touch_point.position) &&
      !ButtonState(gvr::kControllerButtonClick)) {
    state_ = SCROLLING;
    gesture->SetType(blink::WebInputEvent::kGestureScrollBegin);
    UpdateGestureParameters();
    gesture->data.scroll_begin.delta_x_hint =
        displacement_.x() * kDisplacementScaleFactor;
    gesture->data.scroll_begin.delta_y_hint =
        displacement_.y() * kDisplacementScaleFactor;
    gesture->data.scroll_begin.delta_hint_units =
        blink::WebGestureEvent::ScrollUnits::kPrecisePixels;
  }
}

void VrController::HandleScrollingState(blink::WebGestureEvent* gesture) {
  if (touch_info_->touch_up || !(touch_info_->is_touching) ||
      ButtonDownHappened(gvr::kControllerButtonClick)) {
    // Gesture ends.
    gesture->SetType(blink::WebInputEvent::kGestureScrollEnd);
    UpdateGestureParameters();
  } else if (touch_position_changed_) {
    // User continues scrolling and there is a change in touch position.
    gesture->SetType(blink::WebInputEvent::kGestureScrollUpdate);
    UpdateGestureParameters();
    if (IsHorizontalGesture()) {
      gesture->data.scroll_update.delta_x =
          displacement_.x() * kDisplacementScaleFactor;
    } else {
      gesture->data.scroll_update.delta_y =
          displacement_.y() * kDisplacementScaleFactor;
    }
    last_velocity_ = overall_velocity_;
  }
}

bool VrController::IsHorizontalGesture() {
  return std::abs(last_velocity_.x()) > std::abs(last_velocity_.y());
}

bool VrController::InSlop(const gfx::Vector2dF touch_position) {
  return (std::abs(touch_position.x() - init_touch_point_->position.x()) <
          kSlopHorizontal) &&
         (std::abs(touch_position.y() - init_touch_point_->position.y()) <
          kSlopVertical);
}

void VrController::Reset() {
  // Reset state.
  state_ = WAITING;

  // Reset the pointers.
  prev_touch_point_.reset(new TouchPoint);
  cur_touch_point_.reset(new TouchPoint);
  init_touch_point_.reset(new TouchPoint);
  touch_info_.reset(new TouchInfo);
  overall_velocity_ = {0, 0};
  last_velocity_ = {0, 0};
}

void VrController::UpdateGestureParameters() {
  displacement_ =
      touch_info_->touch_point.position - prev_touch_point_->position;
}

bool VrController::UpdateCurrentTouchpoint() {
  touch_info_->touch_up = TouchUpHappened();
  touch_info_->touch_down = TouchDownHappened();
  touch_info_->is_touching = IsTouching();
  touch_info_->touch_point.position.set_x(TouchPosX());
  touch_info_->touch_point.position.set_y(TouchPosY());
  ClampTouchpadPosition(&touch_info_->touch_point.position);
  touch_info_->touch_point.timestamp =
      gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos;

  if (IsTouching() || TouchUpHappened()) {
    // Update the touch point when the touch position has changed.
    if (cur_touch_point_->position != touch_info_->touch_point.position) {
      prev_touch_point_.swap(cur_touch_point_);
      cur_touch_point_.reset(new TouchPoint);
      cur_touch_point_->position = touch_info_->touch_point.position;
      cur_touch_point_->timestamp = touch_info_->touch_point.timestamp;
      return true;
    }
  }
  return false;
}

void VrController::UpdateOverallVelocity() {
  float duration =
      (touch_info_->touch_point.timestamp - prev_touch_point_->timestamp) /
      kNanoSecondsPerSecond;

  // If the timestamp does not change, do not update velocity.
  if (duration < kDelta)
    return;

  const gfx::Vector2dF& displacement =
      touch_info_->touch_point.position - prev_touch_point_->position;

  const gfx::Vector2dF& velocity = ScaleVector2d(displacement, (1 / duration));

  float weight = duration / (kRC + duration);

  overall_velocity_ = ScaleVector2d(overall_velocity_, (1 - weight)) +
                      ScaleVector2d(velocity, weight);
}

}  // namespace vr_shell
