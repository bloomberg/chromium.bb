// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_SCREEN_H_
#define ASH_PUBLIC_CPP_LOGIN_SCREEN_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"

class AccountId;

namespace ash {

class LoginScreenModel;

// Allows clients (e.g. the browser process) to send messages to the ash
// login/lock/user-add screens.
// TODO(estade): move more of mojom::LoginScreen here.
class ASH_PUBLIC_EXPORT LoginScreen {
 public:
  // Returns the singleton instance.
  static LoginScreen* Get();

  virtual LoginScreenModel* GetModel() = 0;

  // Shows or hides the guest button on the login shelf during OOBE.
  virtual void ShowGuestButtonInOobe(bool show) = 0;

  // Shows or hides the parent access button on the login shelf.
  virtual void ShowParentAccessButton(bool show) = 0;

  // Shows a standalone Parent Access dialog. If |child_account_id| is valid, it
  // validates the parent access code for that child only, when it is  empty it
  // validates the code for any child signed in the device. |callback| is
  // invoked when the back button is clicked or the correct code was entered.
  // Note: this is intended for children only. If a non child account id is
  // provided, the validation will necessarily fail.
  virtual void ShowParentAccessWidget(
      const AccountId& child_account_id,
      base::RepeatingCallback<void(bool success)> callback) = 0;

 protected:
  LoginScreen();
  virtual ~LoginScreen();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_SCREEN_H_
