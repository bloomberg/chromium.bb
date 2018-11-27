// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_WINDOW_DELEGATE_H_
#define ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_WINDOW_DELEGATE_H_

#include "ash/ash_export.h"

class AccountId;

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Delegate associated with a single Window. This is used by windows created
// by the WindowService.
class ASH_EXPORT MultiUserWindowManagerWindowDelegate {
 public:
  // Called when the owner of the window tracked by the manager is changed.
  // |was_minimized| is true if the window was minimized. |teleported| is true
  // if the window was not on the desktop of the current user.
  virtual void OnWindowOwnerEntryChanged(aura::Window* window,
                                         const AccountId& account_id,
                                         bool was_minimized,
                                         bool teleported) {}

 protected:
  virtual ~MultiUserWindowManagerWindowDelegate() {}
};

}  // namespace ash

#endif  // ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_WINDOW_DELEGATE_H_
