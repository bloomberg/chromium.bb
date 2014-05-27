// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/users/user.h"

namespace chromeos {

class LoginDisplayWebUIHandler;

// This class represents User Selection screen: user pod-based login screen.
class UserSelectionScreen {
 public:
  UserSelectionScreen();
  virtual ~UserSelectionScreen();

  void SetHandler(LoginDisplayWebUIHandler* handler);

  void Init(const UserList& users);
  const UserList& GetUsers() const;
  void OnUserImageChanged(const User& user);
  void OnBeforeUserRemoved(const std::string& username);
  void OnUserRemoved(const std::string& username);

 private:
  LoginDisplayWebUIHandler* handler_;

  // Set of Users that are visible.
  UserList users_;

  DISALLOW_COPY_AND_ASSIGN(UserSelectionScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_
