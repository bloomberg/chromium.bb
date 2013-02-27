// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/webui_login_display.h"

#include "ash/wm/user_activity_detector.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/wallpaper_manager.h"
#include "chrome/browser/chromeos/login/webui_login_display_host.h"
#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

const int kPasswordClearTimeoutSec = 60;

}

// WebUILoginDisplay, public: --------------------------------------------------

WebUILoginDisplay::~WebUILoginDisplay() {
  if (webui_handler_)
    webui_handler_->ResetSigninScreenHandlerDelegate();
  ash::UserActivityDetector* activity_detector = ash::Shell::GetInstance()->
      user_activity_detector();
  if (activity_detector->HasObserver(this))
    activity_detector->RemoveObserver(this);
}

// LoginDisplay implementation: ------------------------------------------------

WebUILoginDisplay::WebUILoginDisplay(LoginDisplay::Delegate* delegate)
    : LoginDisplay(delegate, gfx::Rect()),
      show_guest_(false),
      show_new_user_(false),
      webui_handler_(NULL) {
}

void WebUILoginDisplay::Init(const UserList& users,
                             bool show_guest,
                             bool show_users,
                             bool show_new_user) {
  // Testing that the delegate has been set.
  DCHECK(delegate_);

  users_ = users;
  show_guest_ = show_guest;
  show_users_ = show_users;
  show_new_user_ = show_new_user;

  ash::UserActivityDetector* activity_detector = ash::Shell::GetInstance()->
      user_activity_detector();
  if (!activity_detector->HasObserver(this))
    activity_detector->AddObserver(this);
}

void WebUILoginDisplay::OnPreferencesChanged() {
  if (webui_handler_)
    webui_handler_->OnPreferencesChanged();
}

void WebUILoginDisplay::OnBeforeUserRemoved(const std::string& username) {
  for (UserList::iterator it = users_.begin(); it != users_.end(); ++it) {
    if ((*it)->email() == username) {
      users_.erase(it);
      break;
    }
  }
}

void WebUILoginDisplay::OnUserImageChanged(const User& user) {
  if (webui_handler_)
    webui_handler_->OnUserImageChanged(user);
}

void WebUILoginDisplay::OnUserRemoved(const std::string& username) {
  if (webui_handler_)
    webui_handler_->OnUserRemoved(username);
}

void WebUILoginDisplay::OnFadeOut() {
}

void WebUILoginDisplay::OnLoginSuccess(const std::string& username) {
  if (webui_handler_)
    webui_handler_->OnLoginSuccess(username);
}

void WebUILoginDisplay::SetUIEnabled(bool is_enabled) {
  // TODO(nkostylev): Cleanup this condition,
  // see http://crbug.com/157885 and http://crbug.com/158255.
  // Allow this call only before user sign in or at lock screen.
  // If this call is made after new user signs in but login screen is still
  // around that would trigger a sign in extension refresh.
  if (webui_handler_ && is_enabled &&
      (!UserManager::Get()->IsUserLoggedIn() ||
       ScreenLocker::default_screen_locker())) {
    webui_handler_->ClearAndEnablePassword();
  }

  if (chromeos::WebUILoginDisplayHost::default_host()) {
    chromeos::WebUILoginDisplayHost* webui_host =
        static_cast<chromeos::WebUILoginDisplayHost*>(
            chromeos::WebUILoginDisplayHost::default_host());
    chromeos::WebUILoginView* login_view = webui_host->login_view();
    if (login_view)
      login_view->SetUIEnabled(is_enabled);
  }
}

void WebUILoginDisplay::SelectPod(int index) {
}

void WebUILoginDisplay::ShowError(int error_msg_id,
                                  int login_attempts,
                                  HelpAppLauncher::HelpTopic help_topic_id) {
  VLOG(1) << "Show error, error_id: " << error_msg_id
          << ", attempts:" << login_attempts
          <<  ", help_topic_id: " << help_topic_id;
  if (!webui_handler_)
    return;

  std::string error_text;
  switch (error_msg_id) {
    case IDS_LOGIN_ERROR_AUTHENTICATING_HOSTED:
      error_text = l10n_util::GetStringFUTF8(
          error_msg_id, l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME));
      break;
    case IDS_LOGIN_ERROR_CAPTIVE_PORTAL:
      error_text = l10n_util::GetStringFUTF8(
          error_msg_id, delegate()->GetConnectedNetworkName());
      break;
    default:
      error_text = l10n_util::GetStringUTF8(error_msg_id);
      break;
  }

  // Only display hints about keyboard layout if the error is authentication-
  // related.
  if (error_msg_id != IDS_LOGIN_ERROR_WHITELIST &&
      error_msg_id != IDS_LOGIN_ERROR_OWNER_KEY_LOST &&
      error_msg_id != IDS_LOGIN_ERROR_OWNER_REQUIRED) {
    // Display a warning if Caps Lock is on.
    input_method::InputMethodManager* ime_manager =
        input_method::GetInputMethodManager();
    if (ime_manager->GetXKeyboard()->CapsLockIsEnabled()) {
      // TODO(ivankr): use a format string instead of concatenation.
      error_text += "\n" +
          l10n_util::GetStringUTF8(IDS_LOGIN_ERROR_CAPS_LOCK_HINT);
    }

    // Display a hint to switch keyboards if there are other active input
    // methods.
    if (ime_manager->GetNumActiveInputMethods() > 1) {
      error_text += "\n" +
          l10n_util::GetStringUTF8(IDS_LOGIN_ERROR_KEYBOARD_SWITCH_HINT);
    }
  }

  std::string help_link;
  switch (error_msg_id) {
    case IDS_LOGIN_ERROR_AUTHENTICATING_HOSTED:
      help_link = l10n_util::GetStringUTF8(IDS_LEARN_MORE);
      break;
    default:
      if (login_attempts > 1)
        help_link = l10n_util::GetStringUTF8(IDS_LEARN_MORE);
      break;
  }

  webui_handler_->ShowError(login_attempts, error_text, help_link,
                            help_topic_id);
  accessibility::MaybeSpeak(error_text);
}

void WebUILoginDisplay::ShowErrorScreen(LoginDisplay::SigninError error_id) {
  VLOG(1) << "Show error screen, error_id: " << error_id;
  if (!webui_handler_)
    return;
  webui_handler_->ShowErrorScreen(error_id);
}

void WebUILoginDisplay::ShowGaiaPasswordChanged(const std::string& username) {
  if (webui_handler_)
    webui_handler_->ShowGaiaPasswordChanged(username);
}

void WebUILoginDisplay::ShowPasswordChangedDialog(bool show_password_error) {
  if (webui_handler_)
    webui_handler_->ShowPasswordChangedDialog(show_password_error);
}

void WebUILoginDisplay::ShowSigninUI(const std::string& email) {
  if (webui_handler_)
    webui_handler_->ShowSigninUI(email);
}

// WebUILoginDisplay, NativeWindowDelegate implementation: ---------------------
gfx::NativeWindow WebUILoginDisplay::GetNativeWindow() const {
  return parent_window();
}

// WebUILoginDisplay, SigninScreenHandlerDelegate implementation: --------------
void WebUILoginDisplay::CancelPasswordChangedFlow() {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->CancelPasswordChangedFlow();
}

void WebUILoginDisplay::CreateAccount() {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->CreateAccount();
}

void WebUILoginDisplay::CompleteLogin(const std::string& username,
                                      const std::string& password) {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->CompleteLogin(username, password);
}

void WebUILoginDisplay::Login(const std::string& username,
                              const std::string& password) {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->Login(username, password);
}

void WebUILoginDisplay::LoginAsRetailModeUser() {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->LoginAsRetailModeUser();
}

void WebUILoginDisplay::LoginAsGuest() {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->LoginAsGuest();
}

void WebUILoginDisplay::LoginAsPublicAccount(const std::string& username) {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->LoginAsPublicAccount(username);
}

void WebUILoginDisplay::MigrateUserData(const std::string& old_password) {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->MigrateUserData(old_password);
}

void WebUILoginDisplay::CreateLocallyManagedUser(const string16& display_name,
                                                 const std::string password) {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->CreateLocallyManagedUser(display_name, password);
}

void WebUILoginDisplay::LoadWallpaper(const std::string& username) {
  WallpaperManager::Get()->SetUserWallpaper(username);
}

void WebUILoginDisplay::LoadSigninWallpaper() {
  WallpaperManager::Get()->SetDefaultWallpaper();
}

void WebUILoginDisplay::RemoveUser(const std::string& username) {
  UserManager::Get()->RemoveUser(username, this);
}

void WebUILoginDisplay::ResyncUserData() {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->ResyncUserData();
}

void WebUILoginDisplay::ShowEnterpriseEnrollmentScreen() {
  if (delegate_)
    delegate_->OnStartEnterpriseEnrollment();
}

void WebUILoginDisplay::ShowResetScreen() {
  if (delegate_)
    delegate_->OnStartDeviceReset();
}

void WebUILoginDisplay::ShowWrongHWIDScreen() {
  if (delegate_)
    delegate_->ShowWrongHWIDScreen();
}

void WebUILoginDisplay::SetWebUIHandler(
    LoginDisplayWebUIHandler* webui_handler) {
  webui_handler_ = webui_handler;
}

void WebUILoginDisplay::ShowSigninScreenForCreds(
    const std::string& username,
    const std::string& password) {
  if (webui_handler_)
    webui_handler_->ShowSigninScreenForCreds(username, password);
}

const UserList& WebUILoginDisplay::GetUsers() const {
  return users_;
}

bool WebUILoginDisplay::IsShowGuest() const {
  return show_guest_;
}

bool WebUILoginDisplay::IsShowUsers() const {
  return show_users_;
}

bool WebUILoginDisplay::IsShowNewUser() const {
  return show_new_user_;
}

void WebUILoginDisplay::SetDisplayEmail(const std::string& email) {
  if (delegate_)
    delegate_->SetDisplayEmail(email);
}

void WebUILoginDisplay::Signout() {
  delegate_->Signout();
}

void WebUILoginDisplay::OnUserActivity() {
  if (!password_clear_timer_.IsRunning())
    StartPasswordClearTimer();
  password_clear_timer_.Reset();
}

void WebUILoginDisplay::StartPasswordClearTimer() {
  DCHECK(!password_clear_timer_.IsRunning());
  password_clear_timer_.Start(FROM_HERE,
      base::TimeDelta::FromSeconds(kPasswordClearTimeoutSec), this,
      &WebUILoginDisplay::OnPasswordClearTimerExpired);
}

void WebUILoginDisplay::OnPasswordClearTimerExpired() {
  if (webui_handler_)
    webui_handler_->ClearUserPodPassword();
}

}  // namespace chromeos
