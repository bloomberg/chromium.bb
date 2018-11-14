// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_DELEGATE_H_
#define ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_DELEGATE_H_

#include "ash/ash_export.h"

class AccountId;

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class ASH_EXPORT MultiUserWindowManagerDelegate {
 public:
  // Called when the owner of the window tracked by the manager is changed.
  // |was_minimized| is true if the window was minimized. |teleported| is true
  // if the window was not on the desktop of the current user.
  virtual void OnOwnerEntryChanged(aura::Window* window,
                                   const AccountId& account_id,
                                   bool was_minimized,
                                   bool teleported) {}

  // Called when the active account changes. This is followed by
  // OnTransitionUserShelfToNewAccount() and OnDidSwitchActiveAccount().
  virtual void OnWillSwitchActiveAccount(const AccountId& account_id) {}

  // Called at the time when the user's shelf should transition to the account
  // supplied to OnWillSwitchActiveAccount().
  virtual void OnTransitionUserShelfToNewAccount() {}

  // Called when the active account change is complete.
  virtual void OnDidSwitchActiveAccount() {}

 protected:
  virtual ~MultiUserWindowManagerDelegate() {}
};

}  // namespace ash

#endif  // ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_DELEGATE_H_
