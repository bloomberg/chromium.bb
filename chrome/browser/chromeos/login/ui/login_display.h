// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

class UserContext;

// TODO(nkostylev): Extract interface, create a BaseLoginDisplay class.
// An abstract class that defines login UI implementation.
class LoginDisplay {
 public:
  // Sign in error IDs that require detailed error screen and not just
  // a simple error bubble.
  enum SigninError {
    // Shown in case of critical TPM error.
    TPM_ERROR,
  };

  class Delegate {
   public:
    // Cancels current password changed flow.
    virtual void CancelPasswordChangedFlow() = 0;

    // Ignore password change, remove existing cryptohome and
    // force full sync of user data.
    virtual void ResyncUserData() = 0;

    // Decrypt cryptohome using user provided |old_password|
    // and migrate to new password.
    virtual void MigrateUserData(const std::string& old_password) = 0;

    // Sign in using |username| and |password| specified.
    // Used for known users only.
    virtual void Login(const UserContext& user_context,
                       const SigninSpecifics& specifics) = 0;

    // Returns true if sign in is in progress.
    virtual bool IsSigninInProgress() const = 0;

    // Sign out the currently signed in user.
    // Used when the lock screen is being displayed.
    virtual void Signout() = 0;

    // Complete sign process with specified |user_context|.
    // Used for new users authenticated through an extension.
    virtual void CompleteLogin(const UserContext& user_context) = 0;

    // Notify the delegate when the sign-in UI is finished loading.
    virtual void OnSigninScreenReady() = 0;

    // Notify the delegate when the GAIA UI is finished loading.
    virtual void OnGaiaScreenReady() = 0;

    // Called when the user requests enterprise enrollment.
    virtual void OnStartEnterpriseEnrollment() = 0;

    // Called when the user requests enable developer features screen.
    virtual void OnStartEnableDebuggingScreen() = 0;

    // Called when the user requests kiosk enable screen.
    virtual void OnStartKioskEnableScreen() = 0;

    // Called when the owner permission for kiosk app auto launch is requested.
    virtual void OnStartKioskAutolaunchScreen() = 0;

    // Shows update required screen.
    virtual void ShowUpdateRequiredScreen() = 0;

    // Shows wrong HWID screen.
    virtual void ShowWrongHWIDScreen() = 0;

    // Sets the displayed email for the next login attempt with |CompleteLogin|.
    // If it succeeds, user's displayed email value will be updated to |email|.
    virtual void SetDisplayEmail(const std::string& email) = 0;
    // Sets the displayed name and given name for the next login attempt with
    // |CompleteLogin|. If it succeeds, user's displayed name and give name
    // values will be updated to |display_name| and |given_name|.
    virtual void SetDisplayAndGivenName(const std::string& display_name,
                                        const std::string& given_name) = 0;

    // Returns name of the currently connected network, for error message,
    virtual base::string16 GetConnectedNetworkName() = 0;

    // Restarts the auto-login timer if it is running.
    virtual void ResetAutoLoginTimer() = 0;

    // Returns true if user is allowed to log in by domain policy.
    virtual bool IsUserWhitelisted(const AccountId& account_id) = 0;

   protected:
    virtual ~Delegate();
  };

  explicit LoginDisplay(Delegate* delegate);
  virtual ~LoginDisplay();

  // Clears and enables fields on user pod or GAIA frame.
  virtual void ClearAndEnablePassword() = 0;

  // Initializes login UI with the user pods based on list of known users and
  // guest, new user pods if those are enabled.
  virtual void Init(const user_manager::UserList& users,
                    bool show_guest,
                    bool show_users,
                    bool show_new_user) = 0;

  // Notifies the login UI that the preferences defining how to visualize it to
  // the user have changed and it needs to refresh.
  virtual void OnPreferencesChanged() = 0;

  // Changes enabled state of the UI.
  virtual void SetUIEnabled(bool is_enabled) = 0;

  // Displays simple error bubble with |error_msg_id| specified.
  // |login_attempts| shows number of login attempts made by current user.
  // |help_topic_id| is additional help topic that is presented as link.
  virtual void ShowError(int error_msg_id,
                         int login_attempts,
                         HelpAppLauncher::HelpTopic help_topic_id) = 0;

  // Displays detailed error screen for error with ID |error_id|.
  virtual void ShowErrorScreen(LoginDisplay::SigninError error_id) = 0;

  // Show password changed dialog. If |show_password_error| is not null
  // user already tried to enter old password but it turned out to be incorrect.
  virtual void ShowPasswordChangedDialog(bool show_password_error,
                                         const std::string& email) = 0;

  // Shows signin UI with specified email.
  virtual void ShowSigninUI(const std::string& email) = 0;

  // Show whitelist check failed error. Happens after user completes online
  // signin but whitelist check fails.
  virtual void ShowWhitelistCheckFailedError() = 0;

  // Show unrecoverable cryptohome error dialog.
  virtual void ShowUnrecoverableCrypthomeErrorDialog() = 0;

  Delegate* delegate() { return delegate_; }
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  gfx::NativeWindow parent_window() const { return parent_window_; }
  void set_parent_window(gfx::NativeWindow window) { parent_window_ = window; }

  bool is_signin_completed() const { return is_signin_completed_; }
  void set_signin_completed(bool value) { is_signin_completed_ = value; }

 protected:
  // Login UI delegate (controller).
  Delegate* delegate_;

  // Parent window, might be used to create dialog windows.
  gfx::NativeWindow parent_window_ = nullptr;

  // True if signin for user has completed.
  // TODO(nkostylev): Find a better place to store this state
  // in redesigned login stack.
  // Login stack (and this object) will be recreated for next user sign in.
  bool is_signin_completed_ = false;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplay);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_H_
