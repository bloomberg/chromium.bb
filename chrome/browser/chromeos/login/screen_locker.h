// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/screen_locker_delegate.h"
#include "ui/base/accelerators/accelerator.h"

namespace chromeos {

class Authenticator;
class LoginFailure;
class User;

namespace test {
class ScreenLockerTester;
}  // namespace test

// ScreenLocker creates a ScreenLockerDelegate which will display the lock UI.
// As well, it takes care of authenticating the user and managing a global
// instance of itself which will be deleted when the system is unlocked.
class ScreenLocker : public LoginStatusConsumer {
 public:
  explicit ScreenLocker(const User& user);

  // Returns the default instance if it has been created.
  static ScreenLocker* default_screen_locker() {
    return screen_locker_;
  }

  // Initialize and show the screen locker.
  void Init();

  // LoginStatusConsumer implements:
  virtual void OnLoginFailure(const chromeos::LoginFailure& error) OVERRIDE;
  virtual void OnLoginSuccess(const std::string& username,
                              const std::string& password,
                              const GaiaAuthConsumer::ClientLoginResult& result,
                              bool pending_requests,
                              bool using_oauth) OVERRIDE;

  // Authenticates the user with given |password| and authenticator.
  void Authenticate(const string16& password);

  // Close message bubble to clear error messages.
  void ClearErrors();

  // (Re)enable input field.
  void EnableInput();

  // Exit the chrome, which will sign out the current session.
  void Signout();

  // Present user a CAPTCHA challenge with image from |captcha_url|,
  // After that shows error bubble with |message|.
  void ShowCaptchaAndErrorMessage(const GURL& captcha_url,
                                  const string16& message);

  // Disables all UI needed and shows error bubble with |message|.
  // If |sign_out_only| is true then all other input except "Sign Out"
  // button is blocked.
  void ShowErrorMessage(const string16& message, bool sign_out_only);

#if defined(TOOLKIT_USES_GTK)
  // Returns the user to authenticate.
  const User& user() const { return user_; }
#endif

  // Allow a LoginStatusConsumer to listen for
  // the same login events that ScreenLocker does.
  void SetLoginStatusConsumer(chromeos::LoginStatusConsumer* consumer);

  // Initialize ScreenLocker class. It will listen to
  // LOGIN_USER_CHANGED notification so that the screen locker accepts
  // lock event only after a user is logged in.
  static void InitClass();

  // Show the screen locker.
  static void Show();

  // Hide the screen locker.
  static void Hide();

  // Queries the value of the webui lock screen flag.
  static bool UseWebUILockScreen();

  // Notifies that PowerManager rejected UnlockScreen request.
  static void UnlockScreenFailed();

#if defined(TOOLKIT_USES_GTK)
  // Returns the tester
  static test::ScreenLockerTester* GetTester();
#endif

 private:
  friend class DeleteTask<ScreenLocker>;
  friend class test::ScreenLockerTester;
  friend class ScreenLockerDelegate;

  virtual ~ScreenLocker();

  // Sets the authenticator.
  void SetAuthenticator(Authenticator* authenticator);

  // Called when the screen lock is ready.
  void ScreenLockReady();

  // ScreenLockerDelegate instance in use.
  scoped_ptr<ScreenLockerDelegate> delegate_;

  // Logged in user.
  const User& user_;

  // Used to authenticate the user to unlock.
  scoped_refptr<Authenticator> authenticator_;

  // Unlock the screen when it detects key/mouse event without asking
  // password. True when chrome is in BWSI or auto login mode.
  bool unlock_on_input_;

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
  LoginStatusConsumer* login_status_consumer_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLocker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_H_
