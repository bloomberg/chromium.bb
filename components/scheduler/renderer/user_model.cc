// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/user_model.h"

namespace scheduler {

UserModel::UserModel() : pending_input_event_count_(0) {}
UserModel::~UserModel() {}

void UserModel::DidStartProcessingInputEvent(blink::WebInputEvent::Type type,
                                             const base::TimeTicks now) {
  last_input_signal_time_ = now;
  if (type == blink::WebInputEvent::TouchStart ||
      type == blink::WebInputEvent::GestureScrollBegin ||
      type == blink::WebInputEvent::GesturePinchBegin) {
    last_gesture_start_time_ = now;
  }

  // We need to track continuous gestures seperatly for scroll detection
  // because taps should not be confused with scrolls.
  if (type == blink::WebInputEvent::GestureScrollBegin ||
      type == blink::WebInputEvent::GestureScrollEnd ||
      type == blink::WebInputEvent::GestureScrollUpdate ||
      type == blink::WebInputEvent::GestureFlingStart ||
      type == blink::WebInputEvent::GestureFlingCancel ||
      type == blink::WebInputEvent::GesturePinchBegin ||
      type == blink::WebInputEvent::GesturePinchEnd ||
      type == blink::WebInputEvent::GesturePinchUpdate) {
    last_continuous_gesture_time_ = now;
  }

  pending_input_event_count_++;
}

void UserModel::DidFinishProcessingInputEvent(const base::TimeTicks now) {
  last_input_signal_time_ = now;
  if (pending_input_event_count_ > 0)
    pending_input_event_count_--;
}

base::TimeDelta UserModel::TimeLeftInUserGesture(base::TimeTicks now) const {
  base::TimeDelta escalated_priority_duration =
      base::TimeDelta::FromMilliseconds(kGestureEstimationLimitMillis);

  // If the input event is still pending, go into input prioritized policy and
  // check again later.
  if (pending_input_event_count_ > 0)
    return escalated_priority_duration;
  if (last_input_signal_time_.is_null() ||
      last_input_signal_time_ + escalated_priority_duration < now) {
    return base::TimeDelta();
  }
  return last_input_signal_time_ + escalated_priority_duration - now;
}

bool UserModel::IsGestureExpectedSoon(
    RendererScheduler::UseCase use_case,
    const base::TimeTicks now,
    base::TimeDelta* prediction_valid_duration) const {
  if (use_case == RendererScheduler::UseCase::NONE) {
    // If we've scrolled recently then future scrolling is likely.
    base::TimeDelta expect_subsequent_gesture_for =
        base::TimeDelta::FromMilliseconds(kExpectSubsequentGestureMillis);
    if (last_continuous_gesture_time_.is_null() ||
        last_continuous_gesture_time_ + expect_subsequent_gesture_for <= now) {
      return false;
    }
    *prediction_valid_duration =
        last_continuous_gesture_time_ + expect_subsequent_gesture_for - now;
    return true;
  }

  if (use_case == RendererScheduler::UseCase::COMPOSITOR_GESTURE ||
      use_case == RendererScheduler::UseCase::MAIN_THREAD_GESTURE) {
    // If we've only just started scrolling then, then initiating a subsequent
    // gesture is unlikely.
    base::TimeDelta minimum_typical_scroll_duration =
        base::TimeDelta::FromMilliseconds(kMinimumTypicalScrollDurationMillis);
    if (last_gesture_start_time_.is_null() ||
        last_gesture_start_time_ + minimum_typical_scroll_duration <= now) {
      return true;
    }
    *prediction_valid_duration =
        last_gesture_start_time_ + minimum_typical_scroll_duration - now;
    return false;
  }
  return false;
}

void UserModel::Reset() {
  last_input_signal_time_ = base::TimeTicks();
  last_gesture_start_time_ = base::TimeTicks();
  last_continuous_gesture_time_ = base::TimeTicks();
}

void UserModel::AsValueInto(base::trace_event::TracedValue* state) const {
  state->BeginDictionary("user_model");
  state->SetInteger("pending_input_event_count", pending_input_event_count_);
  state->SetDouble(
      "last_input_signal_time",
      (last_input_signal_time_ - base::TimeTicks()).InMillisecondsF());
  state->SetDouble(
      "last_touchstart_time",
      (last_gesture_start_time_ - base::TimeTicks()).InMillisecondsF());
  state->EndDictionary();
}

}  // namespace scheduler
