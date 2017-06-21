// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_SCREEN_H_
#define ASH_LOGIN_UI_LOCK_SCREEN_H_

#include "ash/ash_export.h"
#include "base/macros.h"

class AccountId;

namespace ash {

class LockWindow;

class LockScreen {
 public:
  // Fetch the global lock screen instance. |Show()| must have been called
  // before this.
  static LockScreen* Get();

  // Creates and displays the lock screen. The lock screen communicates with the
  // backend C++ via a mojo API.
  static void Show();

  // Destroys an existing lock screen instance.
  void Destroy();

  // Enable or disable PIN for the given user.
  void SetPinEnabledForUser(const AccountId& account_id, bool is_enabled);

 private:
  LockScreen();
  ~LockScreen();

  // Unowned pointer to the window which hosts the lock screen.
  LockWindow* window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LockScreen);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_SCREEN_H_
