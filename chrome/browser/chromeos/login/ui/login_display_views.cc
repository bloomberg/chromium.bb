// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_display_views.h"

#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_views.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "components/user_manager/known_user.h"

namespace chromeos {

namespace {

// TODO(jdufault): Deduplicate this and
// user_selection_screen::GetOwnerAccountId().
AccountId GetOwnerAccountId() {
  std::string owner_email;
  chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner,
                                           &owner_email);
  const AccountId owner = user_manager::known_user::GetAccountId(
      owner_email, std::string() /*id*/, AccountType::UNKNOWN);
  return owner;
}

}  // namespace

LoginDisplayViews::LoginDisplayViews(Delegate* delegate,
                                     LoginDisplayHostViews* host)
    : LoginDisplay(delegate), host_(host) {}

LoginDisplayViews::~LoginDisplayViews() = default;

void LoginDisplayViews::ClearAndEnablePassword() {}

void LoginDisplayViews::Init(const user_manager::UserList& filtered_users,
                             bool show_guest,
                             bool show_users,
                             bool show_new_user) {
  host_->SetUsers(filtered_users);

  // Convert |filtered_users| to mojo structures.
  const AccountId owner_account = GetOwnerAccountId();
  std::vector<ash::mojom::LoginUserInfoPtr> users;
  for (user_manager::User* user : filtered_users) {
    auto mojo_user = ash::mojom::LoginUserInfo::New();

    const bool is_owner = user->GetAccountId() == owner_account;

    const bool is_public_account =
        user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT;
    const proximity_auth::mojom::AuthType initial_auth_type =
        is_public_account
            ? proximity_auth::mojom::AuthType::EXPAND_THEN_USER_CLICK
            : (chromeos::UserSelectionScreen::ShouldForceOnlineSignIn(user)
                   ? proximity_auth::mojom::AuthType::ONLINE_SIGN_IN
                   : proximity_auth::mojom::AuthType::OFFLINE_PASSWORD);

    chromeos::UserSelectionScreen::FillUserMojoStruct(
        user, is_owner, false /*is_signin_to_add*/, initial_auth_type,
        mojo_user.get());

    users.push_back(std::move(mojo_user));
  }

  // Load the login screen.
  auto* client = LoginScreenClient::Get();
  client->SetDelegate(host_);
  client->ShowLoginScreen(
      base::BindOnce([](bool did_show) { CHECK(did_show); }));
  client->LoadUsers(std::move(users), show_guest);
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
