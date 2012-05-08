// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/webui_login_display.h"

#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

// WebUILoginDisplay, public: --------------------------------------------------

WebUILoginDisplay::~WebUILoginDisplay() {
  if (webui_handler_)
    webui_handler_->ResetSigninScreenHandlerDelegate();
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
  if (webui_handler_ && is_enabled)
    webui_handler_->ClearAndEnablePassword();
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
          error_msg_id, l10n_util::GetStringUTF16(IDS_PRODUCT_OS_NAME));
      break;
    case IDS_LOGIN_ERROR_CAPTIVE_PORTAL:
      error_text = l10n_util::GetStringFUTF8(
          error_msg_id, delegate()->GetConnectedNetworkName());
      break;
    default:
      error_text = l10n_util::GetStringUTF8(error_msg_id);
      break;
  }

  // Display a warning if Caps Lock is on and error is authentication-related.
  input_method::InputMethodManager* ime_manager =
      input_method::InputMethodManager::GetInstance();
  if (ime_manager->GetXKeyboard()->CapsLockIsEnabled() &&
      error_msg_id != IDS_LOGIN_ERROR_WHITELIST) {
    // TODO(ivankr): use a format string instead of concatenation.
    error_text += "\n" +
        l10n_util::GetStringUTF8(IDS_LOGIN_ERROR_CAPS_LOCK_HINT);
  }

  // Display a hint to switch keyboards if there are other active input methods
  // and error is authentication-related.
  if (ime_manager->GetNumActiveInputMethods() > 1 &&
      error_msg_id != IDS_LOGIN_ERROR_WHITELIST) {
    error_text += "\n" +
        l10n_util::GetStringUTF8(IDS_LOGIN_ERROR_KEYBOARD_SWITCH_HINT);
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

void WebUILoginDisplay::ShowGaiaPasswordChanged(const std::string& username) {
  if (webui_handler_)
    webui_handler_->ShowGaiaPasswordChanged(username);
}

// WebUILoginDisplay, SigninScreenHandlerDelegate implementation: --------------
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

void WebUILoginDisplay::LoginAsDemoUser() {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->LoginAsDemoUser();
}

void WebUILoginDisplay::LoginAsGuest() {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->LoginAsGuest();
}

void WebUILoginDisplay::Signout() {
  delegate_->Signout();
}

void WebUILoginDisplay::CreateAccount() {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->CreateAccount();
}

void WebUILoginDisplay::UserSelected(const std::string& username) {
  UserManager::Get()->UserSelected(username);
}

void WebUILoginDisplay::RemoveUser(const std::string& username) {
  UserManager::Get()->RemoveUser(username, this);
}

void WebUILoginDisplay::ShowEnterpriseEnrollmentScreen() {
  if (delegate_)
    delegate_->OnStartEnterpriseEnrollment();
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

}  // namespace chromeos
