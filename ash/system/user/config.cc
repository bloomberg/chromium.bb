// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/config.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"

namespace ash {
namespace tray {

namespace {

// Returns true if session is blocked by e.g. the login screen.
bool IsUserSessionBlocked() {
  return Shell::GetInstance()
             ->session_state_delegate()
             ->IsUserSessionBlocked();
}

}  // namespace

bool IsMultiProfileSupportedAndUserActive() {
  return Shell::GetInstance()->delegate()->IsMultiProfilesEnabled() &&
         !IsUserSessionBlocked();
}

bool IsMultiAccountSupportedAndUserActive() {
  return Shell::GetInstance()->delegate()->IsMultiAccountEnabled() &&
         !IsUserSessionBlocked();
}

}  // namespace tray
}  // namespace ash
