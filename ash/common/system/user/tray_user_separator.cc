// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/user/tray_user_separator.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/wm_shell.h"
#include "ui/views/view.h"

namespace ash {

TrayUserSeparator::TrayUserSeparator(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_NOT_RECORDED), separator_shown_(false) {}

views::View* TrayUserSeparator::CreateDefaultView(LoginStatus status) {
  if (status == LoginStatus::NOT_LOGGED_IN)
    return NULL;

  const SessionStateDelegate* session_state_delegate =
      WmShell::Get()->GetSessionStateDelegate();

  // If the screen is locked, a system modal dialog or a single user is shown,
  // show nothing.
  if (session_state_delegate->IsUserSessionBlocked() ||
      WmShell::Get()->IsSystemModalWindowOpen() ||
      session_state_delegate->NumberOfLoggedInUsers() < 2)
    return NULL;

  separator_shown_ = true;
  return new views::View();
}

void TrayUserSeparator::DestroyDefaultView() {
  separator_shown_ = false;
}

}  // namespace ash
