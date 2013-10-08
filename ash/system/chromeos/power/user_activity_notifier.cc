// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/user_activity_notifier.h"

#include "ash/shell.h"
#include "ash/wm/user_activity_detector.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {
namespace internal {

namespace {

// Minimum number of seconds between notifications.
const int kNotifyIntervalSec = 5;

}  // namespace

UserActivityNotifier::UserActivityNotifier(UserActivityDetector* detector)
    : detector_(detector) {
  detector_->AddObserver(this);
}

UserActivityNotifier::~UserActivityNotifier() {
  detector_->RemoveObserver(this);
}

void UserActivityNotifier::OnUserActivity(const ui::Event* event) {
  base::TimeTicks now = base::TimeTicks::Now();
  // InSeconds() truncates rather than rounding, so it's fine for this
  // comparison.
  if (last_notify_time_.is_null() ||
      (now - last_notify_time_).InSeconds() >= kNotifyIntervalSec) {
    power_manager::UserActivityType type = power_manager::USER_ACTIVITY_OTHER;
    if (event && event->type() == ui::ET_KEY_PRESSED) {
      switch (static_cast<const ui::KeyEvent*>(event)->key_code()) {
        case ui::VKEY_BRIGHTNESS_UP:
          type = power_manager::USER_ACTIVITY_BRIGHTNESS_UP_KEY_PRESS;
          break;
        case ui::VKEY_BRIGHTNESS_DOWN:
          type = power_manager::USER_ACTIVITY_BRIGHTNESS_DOWN_KEY_PRESS;
          break;
        default:
          break;
      }
    }

    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
        NotifyUserActivity(type);
    last_notify_time_ = now;
  }
}

}  // namespace internal
}  // namespace ash
