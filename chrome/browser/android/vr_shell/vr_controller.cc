// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_controller.h"

#include <cmath>

#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr_controller.h"

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
constexpr float kSlopHorizontal = 0.125f;

// Minimum distance needed in at least one direction to call two vectors
// not equal. Also, minimum time distance needed to call two timestamps
// not equal.
constexpr float kDelta = 1.0e-7f;

constexpr float kCutoffHz = 10.0f;
constexpr float kRC = static_cast<float>(1.0 / (2.0 * M_PI * kCutoffHz));
constexpr float kNanoSecondsPerSecond = 1.0e9f;

class Vector {
 public:
  static inline void ClampTouchpadPosition(gvr::Vec2f* position) {
    position->x = std::min(std::max(0.0f, position->x), 1.0f);
    position->y = std::min(std::max(0.0f, position->y), 1.0f);
  }

  static inline void SetZero(gvr::Vec2f* v) {
    v->x = 0;
    v->y = 0;
  }

  static inline gvr::Vec2f Subtract(gvr::Vec2f v1, gvr::Vec2f v2) {
    gvr::Vec2f result;
    result.x = v1.x - v2.x;
    result.y = v1.y - v2.y;
    return result;
  }

  static inline gvr::Vec2f Add(gvr::Vec2f v1, gvr::Vec2f v2) {
    gvr::Vec2f result;
    result.x = v1.x + v2.x;
    result.y = v1.y + v2.y;
    return result;
  }

  static inline bool Equal(const gvr::Vec2f v1, const gvr::Vec2f v2) {
    return (std::abs(v1.x - v2.x) < kDelta) && (std::abs(v1.y - v2.y) < kDelta);
  }

  static inline gvr::Vec2f ScalarMult(gvr::Vec2f v, float scalar) {
    gvr::Vec2f vect_prod;
    vect_prod.x = v.x * scalar;
    vect_prod.y = v.y * scalar;
    return vect_prod;
  }

};  // Vector

}  // namespace

VrController::VrController(gvr_context* vr_context) {
  Initialize(vr_context);
  Reset();
}

VrController::~VrController() {}

void VrController::OnResume() {
  if (controller_api_)
    controller_api_->Resume();
}

void VrController::OnPause() {
  if (controller_api_)
    controller_api_->Pause();
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

const gvr::Quatf VrController::Orientation() {
  return controller_state_->GetOrientation();
}

bool VrController::IsTouchDown() {
  return controller_state_->GetTouchDown();
}

bool VrController::IsTouchUp() {
  return controller_state_->GetTouchUp();
}

bool VrController::IsButtonDown(gvr::ControllerButton button) {
  return controller_state_->GetButtonDown(button);
}

bool VrController::IsButtonUp(gvr::ControllerButton button) {
  return controller_state_->GetButtonUp(button);
}

bool VrController::IsConnected() {
  return controller_state_->GetConnectionState() == gvr::kControllerConnected;
}

void VrController::UpdateState() {
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
}

void VrController::UpdateTouchInfo() {
  CHECK(touch_info_ != nullptr) << "touch_info_ not initialized properly.";
  gvr::Vec2f position;
  position.x = TouchPosX();
  position.y = TouchPosY();
  touch_info_->touch_up = IsTouchUp();
  touch_info_->touch_down = IsTouchDown();
  touch_info_->is_touching = IsTouching();
  touch_info_->touch_point.position = position;
  Vector::ClampTouchpadPosition(&touch_info_->touch_point.position);
  touch_info_->touch_point.timestamp =
      gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos;
  if (controller_state_->GetLastTouchTimestamp() == last_touch_timestamp_) {
    // Fill the touch_info
    float duration =
        (gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos -
         last_timestamp_nanos_) /
        kNanoSecondsPerSecond;

    position.x += overall_velocity_.x * duration;
    position.y += overall_velocity_.y * duration;
    touch_info_->touch_point.position.x = position.x;
    touch_info_->touch_point.position.y = position.y;
  }
  last_touch_timestamp_ = controller_state_->GetLastTouchTimestamp();
  last_timestamp_nanos_ =
      gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos;
}

void VrController::Initialize(gvr_context* gvr_context) {
  CHECK(gvr_context != nullptr) << "invalid gvr_context";
  controller_api_.reset(new gvr::ControllerApi);
  controller_state_.reset(new gvr::ControllerState);
  int32_t options = gvr::ControllerApi::DefaultOptions();

  // Enable non-default options, if you need them:
  // options |= GVR_CONTROLLER_ENABLE_GYRO;
  CHECK(controller_api_->Init(options, gvr_context));
  controller_api_->Resume();
}

std::vector<std::unique_ptr<WebGestureEvent>> VrController::DetectGestures() {
  std::vector<std::unique_ptr<WebGestureEvent>> gesture_list;
  std::unique_ptr<WebGestureEvent> gesture(new WebGestureEvent());

  if (controller_state_->GetConnectionState() != gvr::kControllerConnected) {
    gesture_list.push_back(std::move(gesture));
    return gesture_list;
  }
  UpdateTouchInfo();
  UpdateGestureFromTouchInfo(gesture.get());

  if (gesture->type == WebInputEvent::Undefined &&
      IsButtonDown(gvr::kControllerButtonClick)) {
    gesture->type = WebInputEvent::GestureTapDown;
    gesture->data.tapDown.width = 0;
    gesture->data.tapDown.height = 0;
  }
  gesture->sourceDevice = blink::WebGestureDeviceTouchpad;
  gesture_list.push_back(std::move(gesture));

  if (gesture_list.back()->type == WebInputEvent::GestureScrollEnd) {
    if (IsButtonDown(gvr::kControllerButtonClick)) {
      std::unique_ptr<WebGestureEvent> tap_down(new WebGestureEvent());
      tap_down->type = WebInputEvent::GestureTapDown;
      tap_down->timeStampSeconds = gesture_list.back()->timeStampSeconds;
      tap_down->sourceDevice = blink::WebGestureDeviceTouchpad;
      tap_down->data.tapDown.width = 0;
      tap_down->data.tapDown.height = 0;
      gesture_list.push_back(std::move(tap_down));
    } else {
      std::unique_ptr<WebGestureEvent> fling(new WebGestureEvent());
      fling->timeStampSeconds = gesture_list.back()->timeStampSeconds;
      fling->sourceDevice = blink::WebGestureDeviceTouchpad;
      fling->type = WebInputEvent::GestureFlingStart;
      if (IsHorizontalGesture()) {
        fling->data.flingStart.velocityX =
            overall_velocity_.x * kDisplacementScaleFactor;
      } else {
        fling->data.flingStart.velocityY =
            overall_velocity_.y * kDisplacementScaleFactor;
      }
      gesture_list.push_back(std::move(fling));
    }
    Reset();
  }

  return gesture_list;
}

void VrController::UpdateGestureFromTouchInfo(WebGestureEvent* gesture) {
  gesture->timeStampSeconds =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
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
      LOG(ERROR) << "Wrong gesture detector state: " << state_;
      break;
  }
}

void VrController::HandleWaitingState(WebGestureEvent* gesture) {
  // User puts finger on touch pad (or when the touch down for current gesture
  // is missed, initiate gesture from current touch point).
  if (touch_info_->touch_down || touch_info_->is_touching) {
    // update initial touchpoint
    *init_touch_point_ = touch_info_->touch_point;
    // update current touchpoint
    *cur_touch_point_ = touch_info_->touch_point;
    state_ = TOUCHING;

    gesture->type = WebInputEvent::GestureFlingCancel;
    gesture->data.flingCancel.preventBoosting = false;
  }
}

void VrController::HandleDetectingState(WebGestureEvent* gesture) {
  // User lifts up finger from touch pad.
  if (touch_info_->touch_up || !(touch_info_->is_touching)) {
    Reset();
    return;
  }

  // Touch position is changed, the touch point moves outside of slop,
  // and the Controller's button is not down.
  if (UpdateCurrentTouchpoint() && touch_info_->is_touching &&
      !InSlop(touch_info_->touch_point.position) &&
      !IsButtonDown(gvr::kControllerButtonClick)) {
    state_ = SCROLLING;
    gesture->type = WebInputEvent::GestureScrollBegin;
    UpdateGesture(gesture);
    gesture->data.scrollBegin.deltaXHint =
        displacement_.x * kDisplacementScaleFactor;
    gesture->data.scrollBegin.deltaYHint =
        displacement_.y * kDisplacementScaleFactor;
  }
}

void VrController::HandleScrollingState(WebGestureEvent* gesture) {
  // Update current touch point.
  bool touch_position_changed = UpdateCurrentTouchpoint();
  if (touch_info_->touch_up || !(touch_info_->is_touching) ||
      IsButtonDown(gvr::kControllerButtonClick)) {
    // Gesture ends.
    gesture->type = WebInputEvent::GestureScrollEnd;
    UpdateGesture(gesture);
  } else if (touch_position_changed) {
    // User continues scrolling and there is a change in touch position.
    gesture->type = WebInputEvent::GestureScrollUpdate;
    UpdateGesture(gesture);
    if (IsHorizontalGesture()) {
      gesture->data.scrollUpdate.deltaX =
          displacement_.x * kDisplacementScaleFactor;
    } else {
      gesture->data.scrollUpdate.deltaY =
          displacement_.y * kDisplacementScaleFactor;
    }
  }
}

bool VrController::IsHorizontalGesture() {
  return std::abs(overall_velocity_.x) > std::abs(overall_velocity_.y);
}

bool VrController::InSlop(const gvr::Vec2f touch_position) {
  return (std::abs(touch_position.x - init_touch_point_->position.x) <
          kSlopHorizontal) &&
         (std::abs(touch_position.y - init_touch_point_->position.y) <
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
  Vector::SetZero(&overall_velocity_);
}

void VrController::UpdateGesture(WebGestureEvent* gesture) {
  if (!gesture)
    LOG(ERROR) << "The gesture pointer is not initiated properly.";
  displacement_ =
      Vector::Subtract(cur_touch_point_->position, prev_touch_point_->position);
}

bool VrController::UpdateCurrentTouchpoint() {
  if (touch_info_->is_touching || touch_info_->touch_up) {
    // Update the touch point when the touch position has changed.
    if (!Vector::Equal(cur_touch_point_->position,
                       touch_info_->touch_point.position)) {
      prev_touch_point_.swap(cur_touch_point_);
      *cur_touch_point_ = touch_info_->touch_point;
      UpdateOverallVelocity();
      return true;
    }
  }
  return false;
}

void VrController::UpdateOverallVelocity() {
  float duration =
      (cur_touch_point_->timestamp - prev_touch_point_->timestamp) /
      kNanoSecondsPerSecond;

  // If the timestamp does not change, do not update velocity.
  if (duration < kDelta)
    return;

  gvr::Vec2f displacement =
      Vector::Subtract(cur_touch_point_->position, prev_touch_point_->position);

  gvr::Vec2f velocity = Vector::ScalarMult(displacement, 1 / duration);

  float weight = duration / (kRC + duration);

  overall_velocity_ =
      Vector::Add(Vector::ScalarMult(overall_velocity_, 1 - weight),
                  Vector::ScalarMult(velocity, weight));
}

}  // namespace vr_shell
