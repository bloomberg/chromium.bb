// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/session/session_state_delegate.h"

namespace ash {

bool SessionStateDelegate::IsInSecondaryLoginScreen() const {
  return GetSessionState() == session_manager::SessionState::LOGIN_SECONDARY;
}

AddUserSessionPolicy SessionStateDelegate::GetAddUserSessionPolicy() const {
  if (!IsMultiProfileAllowedByPrimaryUserPolicy())
    return AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER;

  if (NumberOfLoggedInUsers() >= GetMaximumNumberOfLoggedInUsers())
    return AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED;

  return AddUserSessionPolicy::ALLOWED;
}

}  // namespace ash
