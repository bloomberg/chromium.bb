// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_display_views.h"

#include "chrome/browser/chromeos/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_views.h"
#include "chrome/browser/chromeos/login/user_selection_screen_proxy.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "components/user_manager/known_user.h"

namespace chromeos {

namespace {
constexpr char kLoginDisplay[] = "login";
}  // namespace

LoginDisplayViews::LoginDisplayViews(Delegate* delegate,
                                     LoginDisplayHostViews* host)
    : LoginDisplay(delegate),
      host_(host),
      user_selection_screen_proxy_(
          std::make_unique<UserSelectionScreenProxy>()),
      user_selection_screen_(
          std::make_unique<ChromeUserSelectionScreen>(kLoginDisplay)) {
  user_selection_screen_->SetView(user_selection_screen_proxy_.get());
}

LoginDisplayViews::~LoginDisplayViews() = default;

void LoginDisplayViews::ClearAndEnablePassword() {}

void LoginDisplayViews::Init(const user_manager::UserList& filtered_users,
                             bool show_guest,
                             bool show_users,
                             bool show_new_user) {
  host_->SetUsers(filtered_users);

  // Load the login screen.
  auto* client = LoginScreenClient::Get();
  client->SetDelegate(host_);
  client->login_screen()->ShowLoginScreen(
      base::BindOnce([](bool did_show) { CHECK(did_show); }));

  user_selection_screen_->Init(filtered_users);
  client->login_screen()->LoadUsers(
      user_selection_screen_->UpdateAndReturnUserListForMojo(), show_guest);
  user_selection_screen_->SetUsersLoaded(true /*loaded*/);
}

void LoginDisplayViews::OnPreferencesChanged() {
  NOTIMPLEMENTED();
}

void LoginDisplayViews::SetUIEnabled(bool is_enabled) {
  NOTIMPLEMENTED();
}

void LoginDisplayViews::ShowError(int error_msg_id,
                                  int login_attempts,
                                  HelpAppLauncher::HelpTopic help_topic_id) {
  NOTIMPLEMENTED();
}

void LoginDisplayViews::ShowErrorScreen(LoginDisplay::SigninError error_id) {
  NOTIMPLEMENTED();
}

void LoginDisplayViews::ShowPasswordChangedDialog(bool show_password_error,
                                                  const std::string& email) {
  NOTIMPLEMENTED();
}

void LoginDisplayViews::ShowSigninUI(const std::string& email) {
  NOTIMPLEMENTED();
}

void LoginDisplayViews::ShowWhitelistCheckFailedError() {
  NOTIMPLEMENTED();
}

void LoginDisplayViews::ShowUnrecoverableCrypthomeErrorDialog() {
  NOTIMPLEMENTED();
}

}  // namespace chromeos
