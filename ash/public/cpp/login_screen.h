// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_SCREEN_H_
#define ASH_PUBLIC_CPP_LOGIN_SCREEN_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"

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

 protected:
  LoginScreen();
  virtual ~LoginScreen();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_SCREEN_H_
