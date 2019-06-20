// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_SCREEN_H_
#define ASH_PUBLIC_CPP_LOGIN_SCREEN_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

class LoginScreenClient;
class LoginScreenModel;

// Allows clients (e.g. the browser process) to send messages to the ash
// login/lock/user-add screens.
// TODO(estade): move more of mojom::LoginScreen here.
class ASH_PUBLIC_EXPORT LoginScreen {
 public:
  // Returns the singleton instance.
  static LoginScreen* Get();

  virtual void SetClient(LoginScreenClient* client) = 0;

  virtual LoginScreenModel* GetModel() = 0;

  // Displays the lock screen.
  virtual void ShowLockScreen() = 0;

  // Displays the login screen.
  virtual void ShowLoginScreen() = 0;

  // Display a toast describing the latest kiosk app launch error.
  virtual void ShowKioskAppError(const std::string& message) = 0;

  // Transitions focus to the shelf area. If |reverse|, focuses the status area.
  virtual void FocusLoginShelf(bool reverse) = 0;

  // Returns if the login/lock screen is ready for a password. Currently only
  // used for testing.
  virtual bool IsReadyForPassword() = 0;

  // Sets whether users can be added from the login screen.
  virtual void EnableAddUserButton(bool enable) = 0;

  // Sets whether shutdown button is enabled in the login screen.
  virtual void EnableShutdownButton(bool enable) = 0;

  // Shows or hides the guest button on the login shelf during OOBE.
  virtual void ShowGuestButtonInOobe(bool show) = 0;

  // Shows or hides the parent access button on the login shelf.
  virtual void ShowParentAccessButton(bool show) = 0;

  // Sets if the guest button on the login shelf can be shown. Even if set to
  // true the button may still not be visible.
  virtual void SetAllowLoginAsGuest(bool allow_guest) = 0;

 protected:
  LoginScreen();
  virtual ~LoginScreen();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_SCREEN_H_
