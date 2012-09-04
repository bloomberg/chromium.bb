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
#include "ui/aura/event_filter.h"

namespace ash {

class UserActivityObserver;

// Watches for input events and notifies observers that the user is active.
class ASH_EXPORT UserActivityDetector : public aura::EventFilter {
 public:
  // Minimum amount of time between notifications to observers that a video is
  // playing.
  static const double kNotifyIntervalMs;

  UserActivityDetector();
  virtual ~UserActivityDetector();

  void set_now_for_test(base::TimeTicks now) { now_for_test_ = now; }

  bool HasObserver(UserActivityObserver* observer) const;
  void AddObserver(UserActivityObserver* observer);
  void RemoveObserver(UserActivityObserver* observer);

  // aura::EventFilter implementation.
  virtual bool PreHandleKeyEvent(
      aura::Window* target,
      ui::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(
      aura::Window* target,
      ui::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(
      aura::Window* target,
      ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult PreHandleGestureEvent(
      aura::Window* target,
      ui::GestureEvent* event) OVERRIDE;

 private:
  // Notifies observers if enough time has passed since the last notification.
  void MaybeNotify();

  ObserverList<UserActivityObserver> observers_;

  // Last time at which we notified observers that the user was active.
  base::TimeTicks last_observer_notification_time_;

  // If set, used when the current time is needed.  This can be set by tests to
  // simulate the passage of time.
  base::TimeTicks now_for_test_;

  DISALLOW_COPY_AND_ASSIGN(UserActivityDetector);
};

}  // namespace ash

#endif  // ASH_WM_USER_ACTIVITY_DETECTOR_H_
