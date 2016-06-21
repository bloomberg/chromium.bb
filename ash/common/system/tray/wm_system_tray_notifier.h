// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_WM_SYSTEM_TRAY_NOTIFIER_H_
#define ASH_COMMON_SYSTEM_TRAY_WM_SYSTEM_TRAY_NOTIFIER_H_

#include "ash/ash_export.h"
#include "ash/common/accessibility_types.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class AccessibilityObserver;
class ClockObserver;
struct UpdateInfo;
class UpdateObserver;

// Observer and notification manager for the ash system tray. This version
// contains the observers that have been ported to //ash/common. See also
// ash::SystemTrayNotifier.
// TODO(jamescook): After all the observers have been moved, rename this class
// to SystemTrayNotifier.
class ASH_EXPORT WmSystemTrayNotifier {
 public:
  WmSystemTrayNotifier();
  ~WmSystemTrayNotifier();

  void AddAccessibilityObserver(AccessibilityObserver* observer);
  void RemoveAccessibilityObserver(AccessibilityObserver* observer);

  void AddClockObserver(ClockObserver* observer);
  void RemoveClockObserver(ClockObserver* observer);

  void AddUpdateObserver(UpdateObserver* observer);
  void RemoveUpdateObserver(UpdateObserver* observer);

  void NotifyAccessibilityModeChanged(
      AccessibilityNotificationVisibility notify);

  void NotifyRefreshClock();
  void NotifyDateFormatChanged();
  void NotifySystemClockTimeUpdated();
  void NotifySystemClockCanSetTimeChanged(bool can_set_time);

  void NotifyUpdateRecommended(const UpdateInfo& info);

 private:
  base::ObserverList<AccessibilityObserver> accessibility_observers_;
  base::ObserverList<ClockObserver> clock_observers_;
  base::ObserverList<UpdateObserver> update_observers_;

  DISALLOW_COPY_AND_ASSIGN(WmSystemTrayNotifier);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_WM_SYSTEM_TRAY_NOTIFIER_H_
