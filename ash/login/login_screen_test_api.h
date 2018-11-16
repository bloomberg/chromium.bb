// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_LOGIN_SCREEN_TEST_API_H_
#define ASH_LOGIN_LOGIN_SCREEN_TEST_API_H_

#include "ash/public/interfaces/login_screen_test_api.mojom.h"
#include "base/macros.h"
#include "components/account_id/account_id.h"

namespace ash {

// Allows tests to access private state of the login/lock screens.
class LoginScreenTestApi : public mojom::LoginScreenTestApi {
 public:
  // Creates and binds an instance from a remote request (e.g. from chrome).
  static void BindRequest(mojom::LoginScreenTestApiRequest request);

  LoginScreenTestApi();
  ~LoginScreenTestApi() override;

  // mojom::LoginScreen:
  void IsLockShown(IsLockShownCallback callback) override;
  void SubmitPassword(const AccountId& account_id,
                      const std::string& password,
                      SubmitPasswordCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenTestApi);
};

}  // namespace ash

#endif  // ASH_LOGIN_LOGIN_SCREEN_TEST_API_H_
