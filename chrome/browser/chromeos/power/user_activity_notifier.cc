// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/user_activity_notifier.h"

#include "ash/shell.h"
#include "ash/wm/user_activity_detector.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

namespace {

// Minimum number of seconds between notifications.
const int kNotifyIntervalSec = 5;

}  // namespace

namespace chromeos {

UserActivityNotifier::UserActivityNotifier() {
  ash::Shell::GetInstance()->user_activity_detector()->AddObserver(this);
}

UserActivityNotifier::~UserActivityNotifier() {
  ash::Shell::GetInstance()->user_activity_detector()->RemoveObserver(this);
}

void UserActivityNotifier::OnUserActivity() {
  base::TimeTicks now = base::TimeTicks::Now();
  // InSeconds() truncates rather than rounding, so it's fine for this
  // comparison.
  if (last_notify_time_.is_null() ||
      (now - last_notify_time_).InSeconds() >= kNotifyIntervalSec) {
    DBusThreadManager::Get()->GetPowerManagerClient()->NotifyUserActivity();
    last_notify_time_ = now;
  }
}

}  // namespace chromeos
