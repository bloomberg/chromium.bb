// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/user_activity_detector.h"

#include "ash/wm/user_activity_observer.h"
#include "ui/aura/event.h"

namespace ash {

const double UserActivityDetector::kNotifyIntervalSec = 1.0;

UserActivityDetector::UserActivityDetector() {
}

UserActivityDetector::~UserActivityDetector() {
}

void UserActivityDetector::AddObserver(UserActivityObserver* observer) {
  observers_.AddObserver(observer);
}

void UserActivityDetector::RemoveObserver(UserActivityObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool UserActivityDetector::PreHandleKeyEvent(aura::Window* target,
                                             aura::KeyEvent* event) {
  MaybeNotify();
  return false;
}

bool UserActivityDetector::PreHandleMouseEvent(aura::Window* target,
                                               aura::MouseEvent* event) {
  if (!(event->flags() & ui::EF_IS_SYNTHESIZED))
    MaybeNotify();
  return false;
}

ui::TouchStatus UserActivityDetector::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  MaybeNotify();
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus UserActivityDetector::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  MaybeNotify();
  return ui::GESTURE_STATUS_UNKNOWN;
}

void UserActivityDetector::MaybeNotify() {
  base::TimeTicks now =
      !now_for_test_.is_null() ? now_for_test_ : base::TimeTicks::Now();
  if (last_observer_notification_time_.is_null() ||
      (now - last_observer_notification_time_).InSecondsF() >=
      kNotifyIntervalSec) {
    FOR_EACH_OBSERVER(UserActivityObserver, observers_, OnUserActivity());
    last_observer_notification_time_ = now;
  }
}

}  // namespace ash
