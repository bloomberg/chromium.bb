// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/session_state_delegate.h"

namespace ash {

bool SessionStateDelegate::IsInSecondaryLoginScreen() const {
  return GetSessionState() ==
      ash::SessionStateDelegate::SESSION_STATE_LOGIN_SECONDARY;
}

bool SessionStateDelegate::CanAddUserToMultiProfile(
    SessionStateDelegate::AddUserError* error) const {
  if (!IsMultiProfileAllowedByPrimaryUserPolicy()) {
    if (error)
      *error = ADD_USER_ERROR_NOT_ALLOWED_PRIMARY_USER;
    return false;
  }
  if (NumberOfLoggedInUsers() >= GetMaximumNumberOfLoggedInUsers()) {
    if (error)
      *error = ADD_USER_ERROR_MAXIMUM_USERS_REACHED;
    return false;
  }
  return true;
}

} // namespace ash
