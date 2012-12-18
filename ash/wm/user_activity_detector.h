// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_USER_ACTIVITY_DETECTOR_H_
#define ASH_WM_USER_ACTIVITY_DETECTOR_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "ui/base/events/event_handler.h"

namespace ash {

class UserActivityObserver;

// Watches for input events and notifies observers that the user is active.
class ASH_EXPORT UserActivityDetector : public ui::EventHandler {
 public:
  // Minimum amount of time between notifications to observers.
  static const double kNotifyIntervalMs;

  UserActivityDetector();
  virtual ~UserActivityDetector();

  void set_now_for_test(base::TimeTicks now) { now_for_test_ = now; }

  bool HasObserver(UserActivityObserver* observer) const;
  void AddObserver(UserActivityObserver* observer);
  void RemoveObserver(UserActivityObserver* observer);

  // Called when chrome has received a request to turn of all displays.
  void OnAllOutputsTurnedOff();

  // ui::EventHandler implementation.
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

 private:
  // Notifies observers if enough time has passed since the last notification.
  void MaybeNotify();

  ObserverList<UserActivityObserver> observers_;

  // Last time at which we notified observers that the user was active.
  base::TimeTicks last_observer_notification_time_;

  // If set, used when the current time is needed.  This can be set by tests to
  // simulate the passage of time.
  base::TimeTicks now_for_test_;

  // When this is true, the next mouse event is ignored. This is to
  // avoid mis-detecting a mouse enter event that occurs when turning
  // off display as a user activity.
  bool ignore_next_mouse_event_;

  DISALLOW_COPY_AND_ASSIGN(UserActivityDetector);
};

}  // namespace ash

#endif  // ASH_WM_USER_ACTIVITY_DETECTOR_H_
