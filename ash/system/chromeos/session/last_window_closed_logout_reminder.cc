// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/session/last_window_closed_logout_reminder.h"

#include "ash/common/login_status.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "ash/system/chromeos/session/logout_confirmation_controller.h"
#include "base/time/time.h"

namespace ash {
namespace {
const int kLogoutConfirmationDelayInSeconds = 20;
}

LastWindowClosedLogoutReminder::LastWindowClosedLogoutReminder() {
  WmShell::Get()->system_tray_notifier()->AddLastWindowClosedObserver(this);
}

LastWindowClosedLogoutReminder::~LastWindowClosedLogoutReminder() {
  WmShell::Get()->system_tray_notifier()->RemoveLastWindowClosedObserver(this);
}

void LastWindowClosedLogoutReminder::OnLastWindowClosed() {
  if (WmShell::Get()->system_tray_delegate()->GetUserLoginStatus() !=
      LoginStatus::PUBLIC) {
    return;
  }

  // Ask the user to confirm logout if a public session is in progress and the
  // screen is not locked.
  Shell::GetInstance()->logout_confirmation_controller()->ConfirmLogout(
      base::TimeTicks::Now() +
      base::TimeDelta::FromSeconds(kLogoutConfirmationDelayInSeconds));
}

}  // namespace ash
