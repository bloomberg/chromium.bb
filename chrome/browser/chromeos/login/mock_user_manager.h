// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_USER_MANAGER_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/browser/chromeos/login/user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockUserManager : public UserManager {
 public:
  MockUserManager() {}
  virtual ~MockUserManager() {}

  MOCK_CONST_METHOD0(GetUsers, std::vector<User>());
  MOCK_METHOD0(OffTheRecordUserLoggedIn, void());
  MOCK_METHOD1(UserLoggedIn, void(const std::string&));
  MOCK_METHOD1(RemoveUser, void(const std::string&));
  MOCK_METHOD1(IsKnownUser, bool(const std::string&));
  MOCK_CONST_METHOD0(logged_in_user, const User&());
  MOCK_METHOD0(current_user_is_owner, bool());
  MOCK_METHOD1(set_current_user_is_owner, void(bool));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_USER_MANAGER_H_
