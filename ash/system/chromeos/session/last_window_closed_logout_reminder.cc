// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/session/last_window_closed_logout_reminder.h"

#include "ash/shell.h"
#include "ash/system/chromeos/session/logout_confirmation_controller.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/user/login_status.h"
#include "base/time/time.h"

namespace ash {
namespace {
const int kLogoutConfirmationDelayInSeconds = 20;
}

LastWindowClosedLogoutReminder::LastWindowClosedLogoutReminder() {
  Shell::GetInstance()->system_tray_notifier()->AddLastWindowClosedObserver(
      this);
}

LastWindowClosedLogoutReminder::~LastWindowClosedLogoutReminder() {
  Shell::GetInstance()->system_tray_notifier()->RemoveLastWindowClosedObserver(
      this);
}

void LastWindowClosedLogoutReminder::OnLastWindowClosed() {
  if (Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus() !=
      user::LOGGED_IN_PUBLIC) {
    return;
  }

  // Ask the user to confirm logout if a public session is in progress and the
  // screen is not locked.
  Shell::GetInstance()->logout_confirmation_controller()->ConfirmLogout(
      base::TimeTicks::Now() +
      base::TimeDelta::FromSeconds(kLogoutConfirmationDelayInSeconds));
}

}  // namespace ash
