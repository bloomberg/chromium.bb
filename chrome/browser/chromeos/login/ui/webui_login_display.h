// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_WEBUI_LOGIN_DISPLAY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_WEBUI_LOGIN_DISPLAY_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chrome/browser/ui/webui/chromeos/login/native_window_delegate.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "components/user_manager/user.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

// WebUI-based login UI implementation.
class WebUILoginDisplay : public LoginDisplay,
                          public NativeWindowDelegate,
                          public SigninScreenHandlerDelegate,
                          public ui::UserActivityObserver {
 public:
  explicit WebUILoginDisplay(LoginDisplay::Delegate* delegate);
  ~WebUILoginDisplay() override;

  // LoginDisplay implementation:
  void ClearAndEnablePassword() override;
  void Init(const user_manager::UserList& users,
            bool show_guest,
            bool show_users,
            bool show_new_user) override;
  void OnPreferencesChanged() override;
  void RemoveUser(const std::string& user_id) override;
  void SetUIEnabled(bool is_enabled) override;
  void ShowError(int error_msg_id,
                 int login_attempts,
                 HelpAppLauncher::HelpTopic help_topic_id) override;
  void ShowErrorScreen(LoginDisplay::SigninError error_id) override;
  void ShowPasswordChangedDialog(bool show_password_error,
                                 const std::string& email) override;
  void ShowSigninUI(const std::string& email) override;
  void ShowWhitelistCheckFailedError() override;

  // NativeWindowDelegate implementation:
  gfx::NativeWindow GetNativeWindow() const override;

  // SigninScreenHandlerDelegate implementation:
  void CancelPasswordChangedFlow() override;
  void ResyncUserData() override;
  void MigrateUserData(const std::string& old_password) override;

  void Login(const UserContext& user_context,
             const SigninSpecifics& specifics) override;
  bool IsSigninInProgress() const override;
  void Signout() override;
  void CompleteLogin(const UserContext& user_context) override;

  void OnSigninScreenReady() override;
  void CancelUserAdding() override;
  void LoadWallpaper(const std::string& username) override;
  void LoadSigninWallpaper() override;
  void ShowEnterpriseEnrollmentScreen() override;
  void ShowEnableDebuggingScreen() override;
  void ShowKioskEnableScreen() override;
  void ShowKioskAutolaunchScreen() override;
  void ShowWrongHWIDScreen() override;
  void SetWebUIHandler(LoginDisplayWebUIHandler* webui_handler) override;
  virtual void ShowSigninScreenForCreds(const std::string& username,
                                        const std::string& password);
  bool IsShowGuest() const override;
  bool IsShowUsers() const override;
  bool IsUserSigninCompleted() const override;
  void SetDisplayEmail(const std::string& email) override;

  void HandleGetUsers() override;
  void CheckUserStatus(const std::string& user_id) override;
  bool IsUserWhitelisted(const std::string& user_id) override;

  // ui::UserActivityDetector implementation:
  void OnUserActivity(const ui::Event* event) override;

 private:

  // Whether to show guest login.
  bool show_guest_;

  // Weather to show the user pods or a GAIA sign in.
  // Public sessions are always shown.
  bool show_users_;

  // Whether to show add new user.
  bool show_new_user_;

  // Reference to the WebUI handling layer for the login screen
  LoginDisplayWebUIHandler* webui_handler_;

  DISALLOW_COPY_AND_ASSIGN(WebUILoginDisplay);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_WEBUI_LOGIN_DISPLAY_H_
