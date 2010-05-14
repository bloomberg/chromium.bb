// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_H_

#include <string>

#include "base/task.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/user_manager.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class WidgetGtk;
}  // namespace views

namespace chromeos {

class Authenticator;
class ScreenLockView;

// ScreenLocker creates a background view as well as ScreenLockView to
// authenticate the user. ScreenLocker manages its life cycle and will
// delete itself when it's unlocked.
class ScreenLocker : public LoginStatusConsumer {
 public:
  ScreenLocker(const UserManager::User& user);

  // Initialize and show the screen locker with given |bounds|.
  void Init(const gfx::Rect& bounds);

  // LoginStatusConsumer implements:
  virtual void OnLoginFailure(const std::string& error);
  virtual void OnLoginSuccess(const std::string& username,
                              const std::string& credentials);

  // Authenticates the user with given |password| and authenticator.
  void Authenticate(const string16& password);

  // Sets the authenticator.
  void SetAuthenticator(Authenticator* authenticator);

  // Returns the user to authenticate.
  const UserManager::User& user() const {
    return user_;
  }

 private:
  friend class DeleteTask<ScreenLocker>;

  virtual ~ScreenLocker();

  // The screen locker window.
  views::WidgetGtk* lock_window_;

  // TYPE_CHILD widget to grab the keyboard/mouse input.
  views::WidgetGtk* lock_widget_;

  // A view that accepts password.
  ScreenLockView* screen_lock_view_;

  // Logged in user.
  UserManager::User user_;

  // Used for logging in.
  scoped_refptr<Authenticator> authenticator_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLocker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_H_
