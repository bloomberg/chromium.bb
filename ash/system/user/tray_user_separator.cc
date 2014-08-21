// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/tray_user_separator.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ui/views/view.h"

namespace ash {

TrayUserSeparator::TrayUserSeparator(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      separator_shown_(false) {
}

views::View* TrayUserSeparator::CreateTrayView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayUserSeparator::CreateDefaultView(user::LoginStatus status) {
  if (status == user::LOGGED_IN_NONE)
    return NULL;

  const SessionStateDelegate* session_state_delegate =
      Shell::GetInstance()->session_state_delegate();

  // If the screen is locked, or only a single user is shown, show nothing.
  if (session_state_delegate->IsUserSessionBlocked() ||
      session_state_delegate->NumberOfLoggedInUsers() < 2)
    return NULL;

  separator_shown_ = true;
  return new views::View();
}

views::View* TrayUserSeparator::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayUserSeparator::DestroyDefaultView() {
  separator_shown_ = false;
}

}  // namespace ash
