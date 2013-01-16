// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_DISPLAY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_DISPLAY_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/ui/webui/chromeos/login/native_window_delegate.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
// WebUI-based login UI implementation.
class WebUILoginDisplay : public LoginDisplay,
                          public NativeWindowDelegate,
                          public SigninScreenHandlerDelegate {
 public:
  explicit WebUILoginDisplay(LoginDisplay::Delegate* delegate);
  virtual ~WebUILoginDisplay();

  // LoginDisplay implementation:
  virtual void Init(const UserList& users,
                    bool show_guest,
                    bool show_users,
                    bool show_new_user) OVERRIDE;
  virtual void OnPreferencesChanged() OVERRIDE;
  virtual void OnBeforeUserRemoved(const std::string& username) OVERRIDE;
  virtual void OnUserImageChanged(const User& user) OVERRIDE;
  virtual void OnUserRemoved(const std::string& username) OVERRIDE;
  virtual void OnFadeOut() OVERRIDE;
  virtual void OnLoginSuccess(const std::string& username) OVERRIDE;
  virtual void SetUIEnabled(bool is_enabled) OVERRIDE;
  virtual void SelectPod(int index) OVERRIDE;
  virtual void ShowError(int error_msg_id,
                         int login_attempts,
                         HelpAppLauncher::HelpTopic help_topic_id) OVERRIDE;
  virtual void ShowErrorScreen(LoginDisplay::SigninError error_id) OVERRIDE;
  virtual void ShowGaiaPasswordChanged(const std::string& username) OVERRIDE;
  virtual void ShowPasswordChangedDialog(bool show_password_error) OVERRIDE;

  // NativeWindowDelegate implementation:
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;

  // SigninScreenHandlerDelegate implementation:
  virtual void CancelPasswordChangedFlow() OVERRIDE;
  virtual void CreateAccount() OVERRIDE;
  virtual void CreateLocallyManagedUser(const std::string& username) OVERRIDE;
  virtual void CompleteLogin(const std::string& username,
                             const std::string& password) OVERRIDE;
  virtual void Login(const std::string& username,
                     const std::string& password) OVERRIDE;
  virtual void LoginAsRetailModeUser() OVERRIDE;
  virtual void LoginAsGuest() OVERRIDE;
  virtual void MigrateUserData(const std::string& old_password) OVERRIDE;
  virtual void LoginAsPublicAccount(const std::string& username) OVERRIDE;
  virtual void LoadWallpaper(const std::string& username) OVERRIDE;
  virtual void LoadSigninWallpaper() OVERRIDE;
  virtual void RemoveUser(const std::string& username) OVERRIDE;
  virtual void ResyncUserData() OVERRIDE;
  virtual void ShowEnterpriseEnrollmentScreen() OVERRIDE;
  virtual void ShowResetScreen() OVERRIDE;
  virtual void SetWebUIHandler(
      LoginDisplayWebUIHandler* webui_handler) OVERRIDE;
  virtual void ShowSigninScreenForCreds(const std::string& username,
                                        const std::string& password);
  virtual const UserList& GetUsers() const OVERRIDE;
  virtual bool IsShowGuest() const OVERRIDE;
  virtual bool IsShowUsers() const OVERRIDE;
  virtual bool IsShowNewUser() const OVERRIDE;
  virtual void SetDisplayEmail(const std::string& email) OVERRIDE;
  virtual void Signout() OVERRIDE;

 private:
  // Set of Users that are visible.
  UserList users_;

  // Whether to show guest login.
  bool show_guest_;

  // Weather to show the user pads or a plain credentials dialogue.
  bool show_users_;

  // Whether to show add new user.
  bool show_new_user_;

  // Reference to the WebUI handling layer for the login screen
  LoginDisplayWebUIHandler* webui_handler_;

  DISALLOW_COPY_AND_ASSIGN(WebUILoginDisplay);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_DISPLAY_H_
