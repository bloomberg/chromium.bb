// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_user_manager.h"

namespace chromeos {

MockUserManager::MockUserManager() : user_(NULL) {}

MockUserManager::~MockUserManager() {
  delete user_;
}

const User& MockUserManager::GetLoggedInUser() const {
  return *user_;
}

User& MockUserManager::GetLoggedInUser() {
  return *user_;
}

// Creates a new User instance.
void MockUserManager::SetLoggedInUser(const std::string& email, bool guest) {
  delete user_;
  user_ = new User(email, guest);
}

}  // namespace chromeos
