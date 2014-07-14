// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_SCREEN_LOCKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_SCREEN_LOCKER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_delegate.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chrome/browser/chromeos/login/users/user.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/user_context.h"
#include "ui/base/accelerators/accelerator.h"

namespace content {
class WebUI;
}

namespace gfx {
class Image;
}

namespace chromeos {

class Authenticator;
class ExtendedAuthenticator;
class AuthFailure;
class ScreenlockIconProvider;

namespace test {
class ScreenLockerTester;
class ScreenLockerViewsTester;
class WebUIScreenLockerTester;
}  // namespace test

// ScreenLocker creates a ScreenLockerDelegate which will display the lock UI.
// As well, it takes care of authenticating the user and managing a global
// instance of itself which will be deleted when the system is unlocked.
class ScreenLocker : public AuthStatusConsumer {
 public:
  explicit ScreenLocker(const UserList& users);

  // Returns the default instance if it has been created.
  static ScreenLocker* default_screen_locker() {
    return screen_locker_;
  }

  bool locked() const { return locked_; }

  // Initialize and show the screen locker.
  void Init();

  // AuthStatusConsumer:
  virtual void OnAuthFailure(const chromeos::AuthFailure& error) OVERRIDE;
  virtual void OnAuthSuccess(const UserContext& user_context) OVERRIDE;

  // Does actual unlocking once authentication is successful and all blocking
  // animations are done.
  void UnlockOnLoginSuccess();

  // Authenticates the user with given |user_context|.
  void Authenticate(const UserContext& user_context);

  // Close message bubble to clear error messages.
  void ClearErrors();

  // Exit the chrome, which will sign out the current session.
  void Signout();

  // (Re)enable input field.
  void EnableInput();

  // Disables all UI needed and shows error bubble with |message|.
  // If |sign_out_only| is true then all other input except "Sign Out"
  // button is blocked.
  void ShowErrorMessage(int error_msg_id,
                        HelpAppLauncher::HelpTopic help_topic_id,
                        bool sign_out_only);

  // Returns the screen locker's delegate.
  ScreenLockerDelegate* delegate() const { return delegate_.get(); }

  // Returns the users to authenticate.
  const UserList& users() const { return users_; }

  // Allow a AuthStatusConsumer to listen for
  // the same login events that ScreenLocker does.
  void SetLoginStatusConsumer(chromeos::AuthStatusConsumer* consumer);

  // Returns WebUI associated with screen locker implementation or NULL if
  // there isn't one.
  content::WebUI* GetAssociatedWebUI();

  // Initialize or uninitialize the ScreenLocker class. It listens to
  // NOTIFICATION_SESSION_STARTED so that the screen locker accepts lock
  // requests only after a user has logged in.
  static void InitClass();
  static void ShutDownClass();

  // Handles a request from the session manager to lock the screen.
  static void HandleLockScreenRequest();

  // Show the screen locker.
  static void Show();

  // Hide the screen locker.
  static void Hide();

  // Returns the tester
  static test::ScreenLockerTester* GetTester();

 private:
  friend class base::DeleteHelper<ScreenLocker>;
  friend class test::ScreenLockerTester;
  friend class test::ScreenLockerViewsTester;
  friend class test::WebUIScreenLockerTester;
  friend class ScreenLockerDelegate;

  struct AuthenticationParametersCapture {
    UserContext user_context;
  };

  virtual ~ScreenLocker();

  // Sets the authenticator.
  void SetAuthenticator(Authenticator* authenticator);

  // Called when the screen lock is ready.
  void ScreenLockReady();

  // Called when screen locker is safe to delete.
  static void ScheduleDeletion();

  // Returns true if |username| is found among logged in users.
  bool IsUserLoggedIn(const std::string& username);

  // Looks up user in unlock user list.
  const User* FindUnlockUser(const std::string& user_id);

  // ScreenLockerDelegate instance in use.
  scoped_ptr<ScreenLockerDelegate> delegate_;

  // Users that can unlock the device.
  UserList users_;

  // Used to authenticate the user to unlock.
  scoped_refptr<Authenticator> authenticator_;

  // Used to authenticate the user to unlock supervised users.
  scoped_refptr<ExtendedAuthenticator> extended_authenticator_;

  // True if the screen is locked, or false otherwise.  This changes
  // from false to true, but will never change from true to
  // false. Instead, ScreenLocker object gets deleted when unlocked.
  bool locked_;

  // Reference to the single instance of the screen locker object.
  // This is used to make sure there is only one screen locker instance.
  static ScreenLocker* screen_locker_;

  // The time when the screen locker object is created.
  base::Time start_time_;
  // The time when the authentication is started.
  base::Time authentication_start_time_;

  // Delegate to forward all login status events to.
  // Tests can use this to receive login status events.
  AuthStatusConsumer* auth_status_consumer_;

  // Number of bad login attempts in a row.
  int incorrect_passwords_count_;

  // Copy of parameters passed to last call of OnLoginSuccess for usage in
  // UnlockOnLoginSuccess().
  scoped_ptr<AuthenticationParametersCapture> authentication_capture_;

  // Provider for button icon set by the screenlockPrivate API.
  scoped_ptr<ScreenlockIconProvider> screenlock_icon_provider_;

  base::WeakPtrFactory<ScreenLocker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLocker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_SCREEN_LOCKER_H_
