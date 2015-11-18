// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_SCREEN_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_SCREEN_CONTROLLER_H_

#include "chrome/browser/chromeos/login/screens/gaia_screen.h"
#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/oobe_display.h"
#include "components/user_manager/remove_user_delegate.h"
#include "components/user_manager/user.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace chromeos {

class LoginDisplayWebUIHandler;

// Class that manages control flow between wizard screens. Wizard controller
// interacts with screen controllers to move the user between screens.
class SignInScreenController : public user_manager::RemoveUserDelegate,
                               public content::NotificationObserver {
 public:
  SignInScreenController(OobeDisplay* oobe_display,
                         LoginDisplay::Delegate* login_display_delegate);
  ~SignInScreenController() override;

  // Returns the default wizard controller if it has been created.
  static SignInScreenController* Get() { return instance_; }

  void SetWebUIHandler(LoginDisplayWebUIHandler* webui_handler);

  // Set up the list of users for user selection screen.
  // TODO(antrim): replace with querying for this data.
  void Init(const user_manager::UserList& users, bool show_guest);

  // Called when signin screen is ready.
  void OnSigninScreenReady();

  // Query to send list of users to user selection screen.
  void SendUserList();

  // Runs OAauth token validity check.
  void CheckUserStatus(const std::string& user_id);

  // Query to remove user with specified id.
  // TODO(antrim): move to user selection screen handler.
  void RemoveUser(const std::string& user_id);

  // user_manager::RemoveUserDelegate implementation:
  void OnBeforeUserRemoved(const std::string& username) override;
  void OnUserRemoved(const std::string& username) override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  static SignInScreenController* instance_;

  OobeDisplay* oobe_display_;

  // Reference to the WebUI handling layer for the login screen
  LoginDisplayWebUIHandler* webui_handler_;

  scoped_ptr<GaiaScreen> gaia_screen_;
  scoped_ptr<UserSelectionScreen> user_selection_screen_;

  // Used for notifications during the login process.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SignInScreenController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_SCREEN_CONTROLLER_H_
