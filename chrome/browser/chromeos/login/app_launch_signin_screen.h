// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_APP_LAUNCH_SIGNIN_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_APP_LAUNCH_SIGNIN_SCREEN_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/authenticator.h"
#include "components/user_manager/user.h"

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

  static void SetUserManagerForTesting(UserManager* user_manager);

 private:
  void InitOwnerUserList();
  UserManager* GetUserManager();

  // SigninScreenHandlerDelegate implementation:
  virtual void CancelPasswordChangedFlow() OVERRIDE;
  virtual void CancelUserAdding() OVERRIDE;
  virtual void CreateAccount() OVERRIDE;
  virtual void CompleteLogin(const UserContext& user_context) OVERRIDE;
  virtual void Login(const UserContext& user_context,
                     const SigninSpecifics& specifics) OVERRIDE;
  virtual void MigrateUserData(const std::string& old_password) OVERRIDE;
  virtual void LoadWallpaper(const std::string& username) OVERRIDE;
  virtual void LoadSigninWallpaper() OVERRIDE;
  virtual void OnSigninScreenReady() OVERRIDE;
  virtual void RemoveUser(const std::string& username) OVERRIDE;
  virtual void ResyncUserData() OVERRIDE;
  virtual void ShowEnterpriseEnrollmentScreen() OVERRIDE;
  virtual void ShowKioskEnableScreen() OVERRIDE;
  virtual void ShowKioskAutolaunchScreen() OVERRIDE;
  virtual void ShowWrongHWIDScreen() OVERRIDE;
  virtual void SetWebUIHandler(
      LoginDisplayWebUIHandler* webui_handler) OVERRIDE;
  virtual void ShowSigninScreenForCreds(const std::string& username,
                                        const std::string& password);
  virtual const user_manager::UserList& GetUsers() const OVERRIDE;
  virtual bool IsShowGuest() const OVERRIDE;
  virtual bool IsShowUsers() const OVERRIDE;
  virtual bool IsSigninInProgress() const OVERRIDE;
  virtual bool IsUserSigninCompleted() const OVERRIDE;
  virtual void SetDisplayEmail(const std::string& email) OVERRIDE;
  virtual void Signout() OVERRIDE;
  virtual void HandleGetUsers() OVERRIDE;
  virtual void SetAuthType(
      const std::string& username,
      ScreenlockBridge::LockHandler::AuthType auth_type) OVERRIDE;
  virtual ScreenlockBridge::LockHandler::AuthType GetAuthType(
      const std::string& username) const OVERRIDE;

  // AuthStatusConsumer implementation:
  virtual void OnAuthFailure(const AuthFailure& error) OVERRIDE;
  virtual void OnAuthSuccess(const UserContext& user_context) OVERRIDE;

  OobeUI* oobe_ui_;
  Delegate* delegate_;
  LoginDisplayWebUIHandler* webui_handler_;
  scoped_refptr<Authenticator> authenticator_;

  // This list should have at most one user, and that user should be the owner.
  user_manager::UserList owner_user_list_;

  static UserManager* test_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchSigninScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_APP_LAUNCH_SIGNIN_SCREEN_H_
