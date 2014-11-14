// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_APP_LAUNCH_SIGNIN_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_APP_LAUNCH_SIGNIN_SCREEN_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/authenticator.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

class OobeUI;

// The app launch signin screen shows the user pod of the device owner
// and requires the user to login in order to access the network dialog.
// This screen is quite similar to the standard lock screen, but we do not
// create a new view to superimpose over the desktop.
//
// TODO(tengs): This class doesn't quite follow the idiom of the other
// screen classes, as SigninScreenHandler is very tightly coupled with
// the login screen. We should do some refactoring in this area.
class AppLaunchSigninScreen : public SigninScreenHandlerDelegate,
                              public AuthStatusConsumer {
 public:
  class Delegate {
   public:
    virtual void OnOwnerSigninSuccess() = 0;

   protected:
    virtual ~Delegate() {}
  };

  AppLaunchSigninScreen(OobeUI* oobe_display, Delegate *delegate);
  virtual ~AppLaunchSigninScreen();

  void Show();

  static void SetUserManagerForTesting(user_manager::UserManager* user_manager);

 private:
  void InitOwnerUserList();
  user_manager::UserManager* GetUserManager();

  // SigninScreenHandlerDelegate implementation:
  virtual void CancelPasswordChangedFlow() override;
  virtual void CancelUserAdding() override;
  virtual void CreateAccount() override;
  virtual void CompleteLogin(const UserContext& user_context) override;
  virtual void Login(const UserContext& user_context,
                     const SigninSpecifics& specifics) override;
  virtual void MigrateUserData(const std::string& old_password) override;
  virtual void LoadWallpaper(const std::string& username) override;
  virtual void LoadSigninWallpaper() override;
  virtual void OnSigninScreenReady() override;
  virtual void RemoveUser(const std::string& username) override;
  virtual void ResyncUserData() override;
  virtual void ShowEnterpriseEnrollmentScreen() override;
  virtual void ShowEnableDebuggingScreen() override;
  virtual void ShowKioskEnableScreen() override;
  virtual void ShowKioskAutolaunchScreen() override;
  virtual void ShowWrongHWIDScreen() override;
  virtual void SetWebUIHandler(
      LoginDisplayWebUIHandler* webui_handler) override;
  virtual void ShowSigninScreenForCreds(const std::string& username,
                                        const std::string& password);
  virtual const user_manager::UserList& GetUsers() const override;
  virtual bool IsShowGuest() const override;
  virtual bool IsShowUsers() const override;
  virtual bool IsSigninInProgress() const override;
  virtual bool IsUserSigninCompleted() const override;
  virtual void SetDisplayEmail(const std::string& email) override;
  virtual void Signout() override;
  virtual void HandleGetUsers() override;
  virtual void SetAuthType(
      const std::string& username,
      ScreenlockBridge::LockHandler::AuthType auth_type) override;
  virtual ScreenlockBridge::LockHandler::AuthType GetAuthType(
      const std::string& username) const override;

  // AuthStatusConsumer implementation:
  virtual void OnAuthFailure(const AuthFailure& error) override;
  virtual void OnAuthSuccess(const UserContext& user_context) override;

  OobeUI* oobe_ui_;
  Delegate* delegate_;
  LoginDisplayWebUIHandler* webui_handler_;
  scoped_refptr<Authenticator> authenticator_;

  // This list should have at most one user, and that user should be the owner.
  user_manager::UserList owner_user_list_;

  static user_manager::UserManager* test_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchSigninScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_APP_LAUNCH_SIGNIN_SCREEN_H_
