// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_UI_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_UI_H_

#include "base/basictypes.h"

namespace password_manager {

namespace ui {

// The current state of the password manager's UI.
enum State {
  // The password manager has nothing to do with the current site.
  INACTIVE_STATE,

  // A password has been typed in and submitted successfully. Now we need to
  // display an Omnibox icon, and pop up a bubble asking the user whether
  // they'd like to save the password.
  PENDING_PASSWORD_AND_BUBBLE_STATE,

  // A password is pending, but we don't need to pop up a bubble.
  PENDING_PASSWORD_STATE,

  // A password has been saved and we wish to display UI confirming the save
  // to the user.
  CONFIRMATION_STATE,

  // A password has been autofilled, or has just been saved. The icon needs
  // to be visible, in the management state.
  MANAGE_STATE,

  // The user has blacklisted the site rendered in the current WebContents.
  // The icon needs to be visible, in the blacklisted state.
  BLACKLIST_STATE,
};

// Returns true if |state| represents a pending password.
bool IsPendingState(State state);

// Returns true if this state show cause the bubble to be shown without user
// interaction.
bool IsAutomaticDisplayState(State state);

// Returns the state that the bubble should be in after the automatic display
// occurs.
State GetEndStateForAutomaticState(State state);

}  // namespace ui

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_UI_H_
