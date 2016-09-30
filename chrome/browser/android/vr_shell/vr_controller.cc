// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_controller.h"

#include <android/log.h>

#include <cmath>

#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebInputEvent;

namespace vr_shell {

namespace {

const GestureScroll kDefaultGestureScroll = {{0, 0}, 0, {0, 0}};
const GestureButtonsChange kDefaultGestureButtonsChange = {{0, 0}, 0, 0};
const GestureAngularMove kDefaultGestureAngularMove = {{0, 0, 0}, 0};

constexpr float kDisplacementScaleFactor = 800.0f;

// A slop represents a small rectangular region around the first touch point of
// a gesture.
// If the user does not move outside of the slop, no gesture is detected.
// Gestures start to be detected when the user moves outside of the slop.
// Vertical distance from the border to the center of slop.
constexpr float kSlopVertical = 0.165f;

// Horizontal distance from the border to the center of slop.
constexpr float kSlopHorizontal = 0.125f;

// Minimum distance needed in at least one direction to call two vectors
// not equal.
constexpr float kDelta = 1.0e-7f;

class Vector {
 public:
  static inline void ClampTouchpadPosition(gvr::Vec2f* position) {
    position->x = std::min(std::max(0.0f, position->x), 1.0f);
    position->y = std::min(std::max(0.0f, position->y), 1.0f);
  }

  static inline void VectSetZero(gvr::Vec2f* v) {
    v->x = 0;
    v->y = 0;
  }

  static inline gvr::Vec2f VectSubtract(gvr::Vec2f v1, gvr::Vec2f v2) {
    gvr::Vec2f result;
    result.x = v1.x - v2.x;
    result.y = v1.y - v2.y;
    return result;
  }

  static inline bool VectEqual(const gvr::Vec2f v1, const gvr::Vec2f v2) {
    return (std::abs(v1.x - v2.x) < kDelta) && (std::abs(v1.y - v2.y) < kDelta);
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
  return controller_state_.IsTouching();
}

float VrController::TouchPosX() {
  return controller_state_.GetTouchPos().x;
}

float VrController::TouchPosY() {
  return controller_state_.GetTouchPos().y;
}

const gvr::Quatf VrController::Orientation() {
  return controller_state_.GetOrientation();
}

bool VrController::IsTouchDown() {
  return controller_state_.GetTouchDown();
}

bool VrController::IsTouchUp() {
  return controller_state_.GetTouchUp();
}

bool VrController::IsButtonDown(const int32_t button) {
  return controller_state_.GetButtonDown(button);
}

bool VrController::IsButtonUp(const int32_t button) {
  return controller_state_.GetButtonUp(button);
}

bool VrController::IsConnected() {
  return controller_state_.GetConnectionState() == gvr::kControllerConnected;
}

void VrController::UpdateState() {
  const int32_t old_status = controller_state_.GetApiStatus();
  const int32_t old_connection_state = controller_state_.GetConnectionState();
  // Read current controller state.
  controller_state_.Update(*controller_api_);
  // Print new API status and connection state, if they changed.
  if (controller_state_.GetApiStatus() != old_status ||
      controller_state_.GetConnectionState() != old_connection_state) {
    VLOG(1) << "Controller Connection status: "
            << gvr_controller_connection_state_to_string(
                   controller_state_.GetConnectionState());
  }
}

void VrController::Update(bool touch_up,
                          bool touch_down,
                          bool is_touching,
                          const gvr::Vec2f position,
                          int64_t timestamp) {
  CHECK(touch_info_ != nullptr) << "touch_info_ not initialized properly.";
  touch_info_->touch_up = touch_up;
  touch_info_->touch_down = touch_down;
  touch_info_->is_touching = is_touching;
  touch_info_->touch_point.position = position;
  Vector::ClampTouchpadPosition(&touch_info_->touch_point.position);
  touch_info_->touch_point.timestamp = timestamp;

  UpdateGestureFromTouchInfo();
}

void VrController::Initialize(gvr_context* gvr_context) {
  CHECK(gvr_context != nullptr) << "invalid gvr_context";
  controller_api_.reset(new gvr::ControllerApi);
  int32_t options = gvr::ControllerApi::DefaultOptions();

  // Enable non-default options, if you need them:
  // options |= GVR_CONTROLLER_ENABLE_GYRO;
  CHECK(controller_api_->Init(options, gvr_context));
  controller_api_->Resume();
}

VrGesture VrController::DetectGesture() {
  if (controller_state_.GetConnectionState() == gvr::kControllerConnected) {
    gvr::Vec2f position;
    position.x = TouchPosX();
    position.y = TouchPosY();
    Update(IsTouchUp(), IsTouchDown(), IsTouching(), position,
           gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos);
    if (GetGestureListSize() > 0 &&
        (GetGesturePtr(0)->type == WebInputEvent::GestureScrollBegin ||
         GetGesturePtr(0)->type == WebInputEvent::GestureScrollUpdate ||
         GetGesturePtr(0)->type == WebInputEvent::GestureScrollEnd)) {
      switch (GetGesturePtr(0)->type) {
        case WebInputEvent::GestureScrollBegin:
          return VrGesture(
              kDefaultGestureScroll,
              gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos,
              gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos,
              GetGesturePtr(0)->displacement.x * kDisplacementScaleFactor,
              GetGesturePtr(0)->displacement.y * kDisplacementScaleFactor,
              WebInputEvent::GestureScrollBegin, Orientation());
        case WebInputEvent::GestureScrollUpdate:
          return VrGesture(
              kDefaultGestureScroll,
              gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos,
              gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos,
              GetGesturePtr(0)->displacement.x * kDisplacementScaleFactor,
              GetGesturePtr(0)->displacement.y * kDisplacementScaleFactor,
              WebInputEvent::GestureScrollUpdate, Orientation());
        case WebInputEvent::GestureScrollEnd:
          return VrGesture(
              kDefaultGestureScroll,
              gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos,
              gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos, 0, 0,
              WebInputEvent::GestureScrollEnd, Orientation());
        default:
          LOG(ERROR) << "Unknown Gesture";
          return VrGesture();
      }
    }

    if (IsButtonDown(gvr::kControllerButtonClick)) {
      return VrGesture(
          kDefaultGestureButtonsChange,
          gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos,
          gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos, 1, 0,
          Orientation());
    }
    return VrGesture(kDefaultGestureAngularMove,
                     gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos,
                     gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos,
                     Orientation());
  }
  return VrGesture();
}

void VrController::UpdateGestureFromTouchInfo() {
  // Clear the gesture list.
  gesture_list_.clear();

  switch (state_) {
    // User has not put finger on touch pad.
    case WAITING:
      HandleWaitingState();
      break;
    // User has not started a gesture (by moving out of slop).
    case TOUCHING:
      HandleDetectingState();
      break;
    // User is scrolling on touchpad
    case SCROLLING:
      HandleScrollingState();
      break;
    default:
      LOG(ERROR) << "Wrong gesture detector state: " << state_;
      break;
  }
}

const VrGesture* VrController::GetGesturePtr(const size_t index) {
  CHECK(index < gesture_list_.size()) << "The gesture index exceeds the"
                                         "size of gesture list.";
  return const_cast<VrGesture*>(&gesture_list_[index]);
}

void VrController::Update(const gvr_controller_state* controller_state) {
  // Update touch information.
  CHECK(touch_info_ != nullptr) << "touch_info_ not initialized properly.";
  touch_info_->touch_up = gvr_controller_state_get_touch_up(controller_state);
  touch_info_->touch_down =
      gvr_controller_state_get_touch_down(controller_state);
  touch_info_->is_touching = gvr_controller_state_is_touching(controller_state);
  touch_info_->touch_point.position.x =
      gvr_controller_state_get_touch_pos(controller_state).x;
  touch_info_->touch_point.position.y =
      gvr_controller_state_get_touch_pos(controller_state).y;
  Vector::ClampTouchpadPosition(&(touch_info_->touch_point.position));
  touch_info_->touch_point.timestamp =
      gvr_controller_state_get_last_touch_timestamp(controller_state);

  UpdateGestureFromTouchInfo();
}

void VrController::HandleWaitingState() {
  // User puts finger on touch pad (or when the touch down for current gesture
  // is missed, initiate gesture from current touch point).
  if (touch_info_->touch_down || touch_info_->is_touching) {
    // update initial touchpoint
    *init_touch_point_ = touch_info_->touch_point;
    // update current touchpoint
    *cur_touch_point_ = touch_info_->touch_point;
    state_ = TOUCHING;
  }
}

void VrController::HandleDetectingState() {
  // User lifts up finger from touch pad.
  if (touch_info_->touch_up || !(touch_info_->is_touching)) {
    Reset();
    return;
  }

  // Touch position is changed and the touch point moves outside of slop.
  if (UpdateCurrentTouchpoint() && touch_info_->is_touching &&
      !InSlop(touch_info_->touch_point.position)) {
    state_ = SCROLLING;
    VrGesture gesture;
    gesture.type = WebInputEvent::GestureScrollBegin;
    UpdateGesture(&gesture);
    gesture_list_.push_back(gesture);
  }
}

void VrController::HandleScrollingState() {
  // Update current touch point.
  bool touch_position_changed = UpdateCurrentTouchpoint();
  if (touch_info_->touch_up || !(touch_info_->is_touching)) {  // gesture ends
    VrGesture scroll_end;
    scroll_end.type = WebInputEvent::GestureScrollEnd;
    UpdateGesture(&scroll_end);
    gesture_list_.push_back(scroll_end);

    Reset();
  } else if (touch_position_changed) {  // User continues scrolling and there is
                                        // a change in touch position.
    VrGesture scroll_update;
    scroll_update.type = WebInputEvent::GestureScrollUpdate;
    UpdateGesture(&scroll_update);
    gesture_list_.push_back(scroll_update);
  }
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
  Vector::VectSetZero(&overall_velocity_);
}

void VrController::UpdateGesture(VrGesture* gesture) {
  if (!gesture)
    LOG(ERROR) << "The gesture pointer is not initiated properly.";
  gesture->velocity = overall_velocity_;
  gesture->displacement = Vector::VectSubtract(cur_touch_point_->position,
                                               prev_touch_point_->position);
}

bool VrController::UpdateCurrentTouchpoint() {
  if (touch_info_->is_touching || touch_info_->touch_up) {
    // Update the touch point when the touch position has changed.
    if (!Vector::VectEqual(cur_touch_point_->position,
                           touch_info_->touch_point.position)) {
      prev_touch_point_.swap(cur_touch_point_);
      *cur_touch_point_ = touch_info_->touch_point;

      return true;
    }
  }
  return false;
}

}  // namespace vr_shell
