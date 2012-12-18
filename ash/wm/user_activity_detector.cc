// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/user_activity_detector.h"

#include "ash/wm/property_util.h"
#include "ash/wm/user_activity_observer.h"
#include "ui/base/events/event.h"

namespace ash {

const double UserActivityDetector::kNotifyIntervalMs = 200.0;

UserActivityDetector::UserActivityDetector() : ignore_next_mouse_event_(false) {
}

UserActivityDetector::~UserActivityDetector() {
}

bool UserActivityDetector::HasObserver(UserActivityObserver* observer) const {
  return observers_.HasObserver(observer);
}

void UserActivityDetector::AddObserver(UserActivityObserver* observer) {
  observers_.AddObserver(observer);
}

void UserActivityDetector::RemoveObserver(UserActivityObserver* observer) {
  observers_.RemoveObserver(observer);
}

void UserActivityDetector::OnAllOutputsTurnedOff() {
  ignore_next_mouse_event_ = true;
}

void UserActivityDetector::OnKeyEvent(ui::KeyEvent* event) {
  MaybeNotify();
}

void UserActivityDetector::OnMouseEvent(ui::MouseEvent* event) {
  VLOG_IF(1, ignore_next_mouse_event_) << "ignoring mouse event";
  if (!(event->flags() & ui::EF_IS_SYNTHESIZED) &&
      !ignore_next_mouse_event_)
    MaybeNotify();
  ignore_next_mouse_event_ = false;
}

void UserActivityDetector::OnScrollEvent(ui::ScrollEvent* event) {
  MaybeNotify();
}

void UserActivityDetector::OnTouchEvent(ui::TouchEvent* event) {
  MaybeNotify();
}

void UserActivityDetector::OnGestureEvent(ui::GestureEvent* event) {
  MaybeNotify();
}

void UserActivityDetector::MaybeNotify() {
  base::TimeTicks now =
      !now_for_test_.is_null() ? now_for_test_ : base::TimeTicks::Now();
  if (last_observer_notification_time_.is_null() ||
      (now - last_observer_notification_time_).InMillisecondsF() >=
      kNotifyIntervalMs) {
    FOR_EACH_OBSERVER(UserActivityObserver, observers_, OnUserActivity());
    last_observer_notification_time_ = now;
  }
}

}  // namespace ash
