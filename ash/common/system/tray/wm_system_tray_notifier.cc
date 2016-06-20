// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/wm_system_tray_notifier.h"

#include "ash/common/system/accessibility_observer.h"
#include "ash/common/system/date/clock_observer.h"
#include "ash/common/system/update/update_observer.h"

namespace ash {

WmSystemTrayNotifier::WmSystemTrayNotifier() {}

WmSystemTrayNotifier::~WmSystemTrayNotifier() {}

void WmSystemTrayNotifier::AddAccessibilityObserver(
    AccessibilityObserver* observer) {
  accessibility_observers_.AddObserver(observer);
}

void WmSystemTrayNotifier::RemoveAccessibilityObserver(
    AccessibilityObserver* observer) {
  accessibility_observers_.RemoveObserver(observer);
}

void WmSystemTrayNotifier::AddClockObserver(ClockObserver* observer) {
  clock_observers_.AddObserver(observer);
}

void WmSystemTrayNotifier::RemoveClockObserver(ClockObserver* observer) {
  clock_observers_.RemoveObserver(observer);
}

void WmSystemTrayNotifier::AddUpdateObserver(UpdateObserver* observer) {
  update_observers_.AddObserver(observer);
}

void WmSystemTrayNotifier::RemoveUpdateObserver(UpdateObserver* observer) {
  update_observers_.RemoveObserver(observer);
}

void WmSystemTrayNotifier::NotifyAccessibilityModeChanged(
    ui::AccessibilityNotificationVisibility notify) {
  FOR_EACH_OBSERVER(AccessibilityObserver, accessibility_observers_,
                    OnAccessibilityModeChanged(notify));
}

void WmSystemTrayNotifier::NotifyRefreshClock() {
  FOR_EACH_OBSERVER(ClockObserver, clock_observers_, Refresh());
}

void WmSystemTrayNotifier::NotifyDateFormatChanged() {
  FOR_EACH_OBSERVER(ClockObserver, clock_observers_, OnDateFormatChanged());
}

void WmSystemTrayNotifier::NotifySystemClockTimeUpdated() {
  FOR_EACH_OBSERVER(ClockObserver, clock_observers_,
                    OnSystemClockTimeUpdated());
}

void WmSystemTrayNotifier::NotifySystemClockCanSetTimeChanged(
    bool can_set_time) {
  FOR_EACH_OBSERVER(ClockObserver, clock_observers_,
                    OnSystemClockCanSetTimeChanged(can_set_time));
}

void WmSystemTrayNotifier::NotifyUpdateRecommended(const UpdateInfo& info) {
  FOR_EACH_OBSERVER(UpdateObserver, update_observers_,
                    OnUpdateRecommended(info));
}

}  // namespace ash
