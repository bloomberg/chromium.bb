// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/webui_login_display.h"

#include "ash/shell.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ime/ime_keyboard.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/login/user_names.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/user_activity_detector.h"

namespace chromeos {

// WebUILoginDisplay, public: --------------------------------------------------

WebUILoginDisplay::~WebUILoginDisplay() {
  if (webui_handler_)
    webui_handler_->ResetSigninScreenHandlerDelegate();
  wm::UserActivityDetector* activity_detector = ash::Shell::GetInstance()->
      user_activity_detector();
  if (activity_detector->HasObserver(this))
    activity_detector->RemoveObserver(this);
}

// LoginDisplay implementation: ------------------------------------------------

WebUILoginDisplay::WebUILoginDisplay(LoginDisplay::Delegate* delegate)
    : LoginDisplay(delegate, gfx::Rect()),
      show_guest_(false),
      show_new_user_(false),
      webui_handler_(NULL),
      gaia_screen_(new GaiaScreen()),
      user_selection_screen_(new ChromeUserSelectionScreen()) {
}

void WebUILoginDisplay::ClearAndEnablePassword() {
  if (webui_handler_)
      webui_handler_->ClearAndEnablePassword();
}

void WebUILoginDisplay::Init(const user_manager::UserList& users,
                             bool show_guest,
                             bool show_users,
                             bool show_new_user) {
  // Testing that the delegate has been set.
  DCHECK(delegate_);

  user_selection_screen_->Init(users, show_guest);
  show_guest_ = show_guest;
  show_users_ = show_users;
  show_new_user_ = show_new_user;

  wm::UserActivityDetector* activity_detector = ash::Shell::GetInstance()->
      user_activity_detector();
  if (!activity_detector->HasObserver(this))
    activity_detector->AddObserver(this);
}

// ---- Common methods

// ---- User selection screen methods

void WebUILoginDisplay::OnBeforeUserRemoved(const std::string& username) {
  user_selection_screen_->OnBeforeUserRemoved(username);
}

void WebUILoginDisplay::OnUserRemoved(const std::string& username) {
  user_selection_screen_->OnUserRemoved(username);
}

void WebUILoginDisplay::OnUserImageChanged(const user_manager::User& user) {
  user_selection_screen_->OnUserImageChanged(user);
}

void WebUILoginDisplay::HandleGetUsers() {
  user_selection_screen_->HandleGetUsers();
}

const user_manager::UserList& WebUILoginDisplay::GetUsers() const {
  return user_selection_screen_->GetUsers();
}

// User selection screen, screen lock API

void WebUILoginDisplay::SetAuthType(
    const std::string& username,
    ScreenlockBridge::LockHandler::AuthType auth_type) {
  user_selection_screen_->SetAuthType(username, auth_type);
}

ScreenlockBridge::LockHandler::AuthType WebUILoginDisplay::GetAuthType(
    const std::string& username) const {
  return user_selection_screen_->GetAuthType(username);
}

// ---- Gaia screen methods

// ---- Not yet classified methods

void WebUILoginDisplay::OnPreferencesChanged() {
  if (webui_handler_)
    webui_handler_->OnPreferencesChanged();
}

void WebUILoginDisplay::SetUIEnabled(bool is_enabled) {
  // TODO(nkostylev): Cleanup this condition,
  // see http://crbug.com/157885 and http://crbug.com/158255.
  // Allow this call only before user sign in or at lock screen.
  // If this call is made after new user signs in but login screen is still
  // around that would trigger a sign in extension refresh.
  if (is_enabled && (!user_manager::UserManager::Get()->IsUserLoggedIn() ||
                     ScreenLocker::default_screen_locker())) {
    ClearAndEnablePassword();
  }

  if (chromeos::LoginDisplayHost* host =
          chromeos::LoginDisplayHostImpl::default_host()) {
    if (chromeos::WebUILoginView* login_view = host->GetWebUILoginView())
      login_view->SetUIEnabled(is_enabled);
  }
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
        input_method::InputMethodManager::Get();
    if (ime_manager->GetImeKeyboard()->CapsLockIsEnabled()) {
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

void WebUILoginDisplay::CancelUserAdding() {
  if (!UserAddingScreen::Get()->IsRunning()) {
    LOG(ERROR) << "User adding screen not running.";
    return;
  }
  UserAddingScreen::Get()->Cancel();
}

void WebUILoginDisplay::CreateAccount() {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->CreateAccount();
}

void WebUILoginDisplay::CompleteLogin(const UserContext& user_context) {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->CompleteLogin(user_context);
}

void WebUILoginDisplay::Login(const UserContext& user_context,
                              const SigninSpecifics& specifics) {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->Login(user_context, specifics);
}

void WebUILoginDisplay::MigrateUserData(const std::string& old_password) {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->MigrateUserData(old_password);
}

void WebUILoginDisplay::LoadWallpaper(const std::string& username) {
  WallpaperManager::Get()->SetUserWallpaperDelayed(username);
}

void WebUILoginDisplay::LoadSigninWallpaper() {
  WallpaperManager::Get()->SetDefaultWallpaperDelayed(
      chromeos::login::kSignInUser);
}

void WebUILoginDisplay::OnSigninScreenReady() {
  if (delegate_)
    delegate_->OnSigninScreenReady();
}

void WebUILoginDisplay::RemoveUser(const std::string& username) {
  user_manager::UserManager::Get()->RemoveUser(username, this);
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

void WebUILoginDisplay::ShowKioskEnableScreen() {
  if (delegate_)
    delegate_->OnStartKioskEnableScreen();
}

void WebUILoginDisplay::ShowKioskAutolaunchScreen() {
  if (delegate_)
    delegate_->OnStartKioskAutolaunchScreen();
}

void WebUILoginDisplay::ShowWrongHWIDScreen() {
  if (delegate_)
    delegate_->ShowWrongHWIDScreen();
}

void WebUILoginDisplay::SetWebUIHandler(
    LoginDisplayWebUIHandler* webui_handler) {
  webui_handler_ = webui_handler;
  gaia_screen_->SetHandler(webui_handler_);
  user_selection_screen_->SetHandler(webui_handler_);
}

void WebUILoginDisplay::ShowSigninScreenForCreds(
    const std::string& username,
    const std::string& password) {
  if (webui_handler_)
    webui_handler_->ShowSigninScreenForCreds(username, password);
}

bool WebUILoginDisplay::IsShowGuest() const {
  return show_guest_;
}

bool WebUILoginDisplay::IsShowUsers() const {
  return show_users_;
}

bool WebUILoginDisplay::IsSigninInProgress() const {
  return delegate_->IsSigninInProgress();
}

bool WebUILoginDisplay::IsUserSigninCompleted() const {
  return is_signin_completed();
}

void WebUILoginDisplay::SetDisplayEmail(const std::string& email) {
  if (delegate_)
    delegate_->SetDisplayEmail(email);
}

void WebUILoginDisplay::Signout() {
  delegate_->Signout();
}

void WebUILoginDisplay::OnUserActivity(const ui::Event* event) {
  if (delegate_)
    delegate_->ResetPublicSessionAutoLoginTimer();
}


}  // namespace chromeos
