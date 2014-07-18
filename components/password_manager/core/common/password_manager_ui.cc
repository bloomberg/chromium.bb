// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "components/password_manager/core/common/password_manager_ui.h"

namespace password_manager {

namespace ui {

bool IsPendingState(State state) {
  return state == PENDING_PASSWORD_AND_BUBBLE_STATE ||
         state == PENDING_PASSWORD_STATE;
}

bool IsAutomaticDisplayState(State state) {
  return state == PENDING_PASSWORD_AND_BUBBLE_STATE ||
         state == CONFIRMATION_STATE;
}

State GetEndStateForAutomaticState(State state) {
  DCHECK(IsAutomaticDisplayState(state));
  switch (state) {
    case PENDING_PASSWORD_AND_BUBBLE_STATE:
      return PENDING_PASSWORD_STATE;
      break;
    case CONFIRMATION_STATE:
      return MANAGE_STATE;
      break;
    default:
      NOTREACHED();
      return INACTIVE_STATE;
  }
}

}  // namespace ui

}  // namespace password_manager
