// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/user_activity_detector.h"

#include "ash/wm/property_util.h"
#include "ash/wm/user_activity_observer.h"
#include "ui/base/events/event.h"

namespace ash {

const int UserActivityDetector::kNotifyIntervalMs = 200;

// Too low and mouse events generated at the tail end of reconfiguration
// will be reported as user activity and turn the screen back on; too high
// and we'll ignore legitimate activity.
const int UserActivityDetector::kDisplayPowerChangeIgnoreMouseMs = 1000;

UserActivityDetector::UserActivityDetector() {
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

void UserActivityDetector::OnDisplayPowerChanging() {
  honor_mouse_events_time_ = GetCurrentTime() +
      base::TimeDelta::FromMilliseconds(kDisplayPowerChangeIgnoreMouseMs);
}

void UserActivityDetector::OnKeyEvent(ui::KeyEvent* event) {
  HandleActivity();
}

void UserActivityDetector::OnMouseEvent(ui::MouseEvent* event) {
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return;
  if (!honor_mouse_events_time_.is_null() &&
      GetCurrentTime() < honor_mouse_events_time_)
    return;

  HandleActivity();
}

void UserActivityDetector::OnScrollEvent(ui::ScrollEvent* event) {
  HandleActivity();
}

void UserActivityDetector::OnTouchEvent(ui::TouchEvent* event) {
  HandleActivity();
}

void UserActivityDetector::OnGestureEvent(ui::GestureEvent* event) {
  HandleActivity();
}

base::TimeTicks UserActivityDetector::GetCurrentTime() const {
  return !now_for_test_.is_null() ? now_for_test_ : base::TimeTicks::Now();
}

void UserActivityDetector::HandleActivity() {
  base::TimeTicks now = GetCurrentTime();
  last_activity_time_ = now;
  if (last_observer_notification_time_.is_null() ||
      (now - last_observer_notification_time_).InMillisecondsF() >=
      kNotifyIntervalMs) {
    FOR_EACH_OBSERVER(UserActivityObserver, observers_, OnUserActivity());
    last_observer_notification_time_ = now;
  }
}

}  // namespace ash
