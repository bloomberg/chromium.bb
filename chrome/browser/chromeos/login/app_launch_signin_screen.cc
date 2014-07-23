// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/app_launch_signin_screen.h"

#include "base/values.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

UserManager* AppLaunchSigninScreen::test_user_manager_ = NULL;

AppLaunchSigninScreen::AppLaunchSigninScreen(
    OobeUI* oobe_ui, Delegate* delegate)
    : oobe_ui_(oobe_ui),
      delegate_(delegate),
      webui_handler_(NULL) {
}

AppLaunchSigninScreen::~AppLaunchSigninScreen() {
  oobe_ui_->ResetSigninScreenHandlerDelegate();
}

void AppLaunchSigninScreen::Show() {
  InitOwnerUserList();
  oobe_ui_->web_ui()->CallJavascriptFunction(
      "login.AccountPickerScreen.setShouldShowApps",
      base::FundamentalValue(false));
  oobe_ui_->ShowSigninScreen(LoginScreenContext(), this, NULL);
}

void AppLaunchSigninScreen::InitOwnerUserList() {
  UserManager* user_manager = GetUserManager();
  const std::string& owner_email = user_manager->GetOwnerEmail();
  const user_manager::UserList& all_users = user_manager->GetUsers();

  owner_user_list_.clear();
  for (user_manager::UserList::const_iterator it = all_users.begin();
       it != all_users.end();
       ++it) {
    user_manager::User* user = *it;
    if (user->email() == owner_email) {
      owner_user_list_.push_back(user);
      break;
    }
  }
}

// static
void AppLaunchSigninScreen::SetUserManagerForTesting(
    UserManager* user_manager) {
  test_user_manager_ = user_manager;
}

UserManager* AppLaunchSigninScreen::GetUserManager() {
  return test_user_manager_ ? test_user_manager_ : UserManager::Get();
}

void AppLaunchSigninScreen::CancelPasswordChangedFlow() {
  NOTREACHED();
}

void AppLaunchSigninScreen::CancelUserAdding() {
  NOTREACHED();
}

void AppLaunchSigninScreen::CreateAccount() {
  NOTREACHED();
}

void AppLaunchSigninScreen::CompleteLogin(const UserContext& user_context) {
  NOTREACHED();
}

void AppLaunchSigninScreen::Login(const UserContext& user_context,
                                  const SigninSpecifics& specifics) {
  // Note: LoginUtils::CreateAuthenticator doesn't necessarily create
  // a new Authenticator object, and could reuse an existing one.
  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::AuthenticateToUnlock,
                 authenticator_.get(),
                 user_context));
}

void AppLaunchSigninScreen::MigrateUserData(const std::string& old_password) {
  NOTREACHED();
}

void AppLaunchSigninScreen::LoadWallpaper(const std::string& username) {
}

void AppLaunchSigninScreen::LoadSigninWallpaper() {
}

void AppLaunchSigninScreen::OnSigninScreenReady() {
}

void AppLaunchSigninScreen::RemoveUser(const std::string& username) {
  NOTREACHED();
}

void AppLaunchSigninScreen::ResyncUserData() {
  NOTREACHED();
}

void AppLaunchSigninScreen::ShowEnterpriseEnrollmentScreen() {
  NOTREACHED();
}

void AppLaunchSigninScreen::ShowKioskEnableScreen() {
  NOTREACHED();
}

void AppLaunchSigninScreen::ShowKioskAutolaunchScreen() {
  NOTREACHED();
}

void AppLaunchSigninScreen::ShowWrongHWIDScreen() {
  NOTREACHED();
}

void AppLaunchSigninScreen::SetWebUIHandler(
    LoginDisplayWebUIHandler* webui_handler) {
  webui_handler_ = webui_handler;
}

void AppLaunchSigninScreen::ShowSigninScreenForCreds(
    const std::string& username,
    const std::string& password) {
  NOTREACHED();
}

const user_manager::UserList& AppLaunchSigninScreen::GetUsers() const {
  if (test_user_manager_) {
    return test_user_manager_->GetUsers();
  }
  return owner_user_list_;
}

bool AppLaunchSigninScreen::IsShowGuest() const {
  return false;
}

bool AppLaunchSigninScreen::IsShowUsers() const {
  return true;
}

bool AppLaunchSigninScreen::IsSigninInProgress() const {
  // Return true to suppress network processing in the signin screen.
  return true;
}

bool AppLaunchSigninScreen::IsUserSigninCompleted() const {
  return false;
}

void AppLaunchSigninScreen::SetDisplayEmail(const std::string& email) {
  return;
}

void AppLaunchSigninScreen::Signout() {
  NOTREACHED();
}

void AppLaunchSigninScreen::OnAuthFailure(const AuthFailure& error) {
  LOG(ERROR) << "Unlock failure: " << error.reason();
  webui_handler_->ClearAndEnablePassword();
  webui_handler_->ShowError(
     0,
     l10n_util::GetStringUTF8(IDS_LOGIN_ERROR_AUTHENTICATING_KIOSK),
     std::string(),
     HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT_OFFLINE);
}

void AppLaunchSigninScreen::OnAuthSuccess(const UserContext& user_context) {
  delegate_->OnOwnerSigninSuccess();
}

void AppLaunchSigninScreen::HandleGetUsers() {
  base::ListValue users_list;
  const user_manager::UserList& users = GetUsers();

  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end();
       ++it) {
    ScreenlockBridge::LockHandler::AuthType initial_auth_type =
        UserSelectionScreen::ShouldForceOnlineSignIn(*it)
            ? ScreenlockBridge::LockHandler::ONLINE_SIGN_IN
            : ScreenlockBridge::LockHandler::OFFLINE_PASSWORD;
    base::DictionaryValue* user_dict = new base::DictionaryValue();
    UserSelectionScreen::FillUserDictionary(
        *it, true, false, initial_auth_type, user_dict);
    users_list.Append(user_dict);
  }

  webui_handler_->LoadUsers(users_list, false);
}

void AppLaunchSigninScreen::SetAuthType(
    const std::string& username,
    ScreenlockBridge::LockHandler::AuthType auth_type) {
  return;
}

ScreenlockBridge::LockHandler::AuthType AppLaunchSigninScreen::GetAuthType(
    const std::string& username) const {
  return ScreenlockBridge::LockHandler::OFFLINE_PASSWORD;
}

}  // namespace chromeos
