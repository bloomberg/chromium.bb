// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_VIEWS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_VIEWS_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"

namespace chromeos {

class LoginDisplayHostViews;

// Interface used by UI-agnostic code to send messages to views-based login
// screen.
class LoginDisplayViews : public LoginDisplay {
 public:
  LoginDisplayViews(Delegate* delegate, LoginDisplayHostViews* host);
  ~LoginDisplayViews() override;

  // LoginDisplay:
  void ClearAndEnablePassword() override;
  void Init(const user_manager::UserList& filtered_users,
            bool show_guest,
            bool show_users,
            bool show_new_user) override;
  void OnPreferencesChanged() override;
  void SetUIEnabled(bool is_enabled) override;
  void ShowError(int error_msg_id,
                 int login_attempts,
                 HelpAppLauncher::HelpTopic help_topic_id) override;
  void ShowErrorScreen(LoginDisplay::SigninError error_id) override;
  void ShowPasswordChangedDialog(bool show_password_error,
                                 const std::string& email) override;
  void ShowSigninUI(const std::string& email) override;
  void ShowWhitelistCheckFailedError() override;
  void ShowUnrecoverableCrypthomeErrorDialog() override;

 private:
  LoginDisplayHostViews* const host_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayViews);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_VIEWS_H_
