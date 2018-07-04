// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/gesture_detector.h"

#include "base/numerics/math_constants.h"
#include "chrome/browser/vr/input_event.h"

namespace vr {

namespace {

constexpr float kDisplacementScaleFactor = 129.0f;

constexpr int kMaxNumOfExtrapolations = 2;

// Minimum time distance needed to call two timestamps
// not equal.
constexpr float kDelta = 1.0e-7f;

constexpr float kCutoffHz = 10.0f;
constexpr float kRC = 1.0f / (2.0f * base::kPiFloat * kCutoffHz);

// A slop represents a small rectangular region around the first touch point of
// a gesture.
// If the user does not move outside of the slop, no gesture is detected.
// Gestures start to be detected when the user moves outside of the slop.
// Vertical distance from the border to the center of slop.
constexpr float kSlopVertical = 0.165f;

// Horizontal distance from the border to the center of slop.
constexpr float kSlopHorizontal = 0.15f;

}  // namespace

GestureDetector::GestureDetector() {
  Reset();
}
GestureDetector::~GestureDetector() = default;

std::unique_ptr<InputEventList> GestureDetector::DetectGestures(
    const TouchInfo& input_touch_info,
    base::TimeTicks current_timestamp,
    bool force_cancel) {
  touch_position_changed_ = UpdateCurrentTouchPoint(input_touch_info);
  TouchInfo touch_info = input_touch_info;
  ExtrapolateTouchInfo(&touch_info, current_timestamp);
  if (touch_position_changed_)
    UpdateOverallVelocity(touch_info);

  auto gesture_list = std::make_unique<InputEventList>();
  auto gesture = GetGestureFromTouchInfo(touch_info, force_cancel);

  if (!gesture)
    return gesture_list;

  if (gesture->type() == InputEvent::kScrollEnd)
    Reset();

  if (gesture->type() != InputEvent::kTypeUndefined)
    gesture_list->push_back(std::move(gesture));

  return gesture_list;
}

std::unique_ptr<InputEvent> GestureDetector::GetGestureFromTouchInfo(
    const TouchInfo& touch_info,
    bool force_cancel) {
  std::unique_ptr<InputEvent> gesture;

  switch (state_->label) {
    // User has not put finger on touch pad.
    case WAITING:
      gesture = HandleWaitingState(touch_info);
      break;
    // User has not started a gesture (by moving out of slop).
    case TOUCHING:
      gesture = HandleDetectingState(touch_info, force_cancel);
      break;
    // User is scrolling on touchpad
    case SCROLLING:
      gesture = HandleScrollingState(touch_info, force_cancel);
      break;
    // The user has finished scrolling, but we'll hallucinate a few points
    // before really finishing.
    case POST_SCROLL:
      gesture = HandlePostScrollingState(touch_info, force_cancel);
      break;
    default:
      NOTREACHED();
      break;
  }

  if (gesture)
    gesture->set_time_stamp(touch_info.touch_point.timestamp);

  return gesture;
}

std::unique_ptr<InputEvent> GestureDetector::HandleWaitingState(
    const TouchInfo& touch_info) {
  // User puts finger on touch pad (or when the touch down for current gesture
  // is missed, initiate gesture from current touch point).
  if (touch_info.touch_down || touch_info.is_touching) {
    // update initial touchpoint
    state_->initial_touch_point = touch_info.touch_point;
    // update current touchpoint
    state_->cur_touch_point = touch_info.touch_point;
    state_->label = TOUCHING;

    return std::make_unique<InputEvent>(InputEvent::kFlingCancel);
  }
  return nullptr;
}

std::unique_ptr<InputEvent> GestureDetector::HandleDetectingState(
    const TouchInfo& touch_info,
    bool force_cancel) {
  // User lifts up finger from touch pad.
  if (touch_info.touch_up || !touch_info.is_touching) {
    Reset();
    return nullptr;
  }

  // Touch position is changed, the touch point moves outside of slop,
  // and the Controller's button is not down.
  if (touch_position_changed_ && touch_info.is_touching &&
      !InSlop(touch_info.touch_point.position) && !force_cancel) {
    state_->label = SCROLLING;
    auto gesture = std::make_unique<InputEvent>(InputEvent::kScrollBegin);
    UpdateGestureParameters(touch_info);
    UpdateGestureWithScrollDelta(gesture.get());
    return gesture;
  }
  return nullptr;
}

std::unique_ptr<InputEvent> GestureDetector::HandleScrollingState(
    const TouchInfo& touch_info,
    bool force_cancel) {
  if (force_cancel) {
    UpdateGestureParameters(touch_info);
    return std::make_unique<InputEvent>(InputEvent::kScrollEnd);
  }
  if (touch_info.touch_up || !(touch_info.is_touching)) {
    state_->label = POST_SCROLL;
  }
  if (touch_position_changed_) {
    auto gesture = std::make_unique<InputEvent>(InputEvent::kScrollUpdate);
    UpdateGestureParameters(touch_info);
    UpdateGestureWithScrollDelta(gesture.get());
    return gesture;
  }
  return nullptr;
}

std::unique_ptr<InputEvent> GestureDetector::HandlePostScrollingState(
    const TouchInfo& touch_info,
    bool force_cancel) {
  if (extrapolated_touch_ == 0 || force_cancel) {
    UpdateGestureParameters(touch_info);
    return std::make_unique<InputEvent>(InputEvent::kScrollEnd);
  } else {
    auto gesture = std::make_unique<InputEvent>(InputEvent::kScrollUpdate);
    UpdateGestureParameters(touch_info);
    UpdateGestureWithScrollDelta(gesture.get());
    return gesture;
  }
}

void GestureDetector::UpdateGestureWithScrollDelta(InputEvent* gesture) {
  gesture->scroll_data.delta_x =
      state_->displacement.x() * kDisplacementScaleFactor;
  gesture->scroll_data.delta_y =
      state_->displacement.y() * kDisplacementScaleFactor;
}

bool GestureDetector::UpdateCurrentTouchPoint(const TouchInfo& touch_info) {
  if (touch_info.is_touching || touch_info.touch_up) {
    // Update the touch point when the touch position has changed.
    if (state_->cur_touch_point.position != touch_info.touch_point.position) {
      state_->prev_touch_point = state_->cur_touch_point;
      state_->cur_touch_point = touch_info.touch_point;
      return true;
    }
  }
  return false;
}

void GestureDetector::ExtrapolateTouchInfo(TouchInfo* touch_info,
                                           base::TimeTicks current_timestamp) {
  const bool effectively_scrolling =
      state_->label == SCROLLING || state_->label == POST_SCROLL;
  if (effectively_scrolling && extrapolated_touch_ < kMaxNumOfExtrapolations &&
      (touch_info->touch_point.timestamp == last_touch_timestamp_ ||
       state_->cur_touch_point.position == state_->prev_touch_point.position)) {
    extrapolated_touch_++;
    touch_position_changed_ = true;
    // Fill the touch_info
    float duration = (current_timestamp - last_timestamp_).InSecondsF();
    touch_info->touch_point.position.set_x(
        state_->cur_touch_point.position.x() +
        state_->overall_velocity.x() * duration);
    touch_info->touch_point.position.set_y(
        state_->cur_touch_point.position.y() +
        state_->overall_velocity.y() * duration);
  } else {
    if (extrapolated_touch_ == kMaxNumOfExtrapolations) {
      state_->overall_velocity = {0, 0};
    }
    extrapolated_touch_ = 0;
  }
  last_touch_timestamp_ = touch_info->touch_point.timestamp;
  last_timestamp_ = current_timestamp;
}

void GestureDetector::UpdateOverallVelocity(const TouchInfo& touch_info) {
  float duration =
      (touch_info.touch_point.timestamp - state_->prev_touch_point.timestamp)
          .InSecondsF();
  // If the timestamp does not change, do not update velocity.
  if (duration < kDelta)
    return;

  const gfx::Vector2dF& displacement =
      touch_info.touch_point.position - state_->prev_touch_point.position;

  const gfx::Vector2dF& velocity = ScaleVector2d(displacement, (1 / duration));

  float weight = duration / (kRC + duration);

  state_->overall_velocity =
      ScaleVector2d(state_->overall_velocity, (1 - weight)) +
      ScaleVector2d(velocity, weight);
}

void GestureDetector::UpdateGestureParameters(const TouchInfo& touch_info) {
  state_->displacement =
      touch_info.touch_point.position - state_->prev_touch_point.position;
}

bool GestureDetector::InSlop(const gfx::Vector2dF touch_position) const {
  return (std::abs(touch_position.x() -
                   state_->initial_touch_point.position.x()) <
          kSlopHorizontal) &&
         (std::abs(touch_position.y() -
                   state_->initial_touch_point.position.y()) < kSlopVertical);
}

void GestureDetector::Reset() {
  state_ = std::make_unique<GestureDetectorState>();
}

}  // namespace vr
