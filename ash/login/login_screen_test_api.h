// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_LOGIN_SCREEN_TEST_API_H_
#define ASH_LOGIN_LOGIN_SCREEN_TEST_API_H_

#include <string>

#include "ash/public/interfaces/login_screen_test_api.test-mojom.h"
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
  void IsLoginShelfShown(IsLoginShelfShownCallback callback) override;
  void IsRestartButtonShown(IsRestartButtonShownCallback callback) override;
  void IsShutdownButtonShown(IsShutdownButtonShownCallback callback) override;
  void IsAuthErrorBubbleShown(IsAuthErrorBubbleShownCallback callback) override;
  void IsGuestButtonShown(IsGuestButtonShownCallback callback) override;
  void IsAddUserButtonShown(IsAddUserButtonShownCallback callback) override;
  void SubmitPassword(const AccountId& account_id,
                      const std::string& password,
                      SubmitPasswordCallback callback) override;
  void GetUiUpdateCount(GetUiUpdateCountCallback callback) override;
  void LaunchApp(const std::string& app_id,
                 LaunchAppCallback callback) override;
  void ClickAddUserButton(ClickAddUserButtonCallback callback) override;
  void ClickGuestButton(ClickGuestButtonCallback callback) override;
  // This blocks until UI update number becomes greater than the
  // |previous_update_count|.  Where |previous_update_count| presumably is
  // coming from GetUiUpdateCount().  This way test remembers the "current" UI
  // update version, performs some actions, and then waits until UI switches to
  // the new version.
  void WaitForUiUpdate(int64_t previous_update_count,
                       WaitForUiUpdateCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenTestApi);
};

}  // namespace ash

#endif  // ASH_LOGIN_LOGIN_SCREEN_TEST_API_H_
