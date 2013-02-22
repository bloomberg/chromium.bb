// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_user_manager.h"

namespace chromeos {

MockUserManager::MockUserManager() : user_(NULL),
                                     user_flow_(new DefaultUserFlow()) {}

MockUserManager::~MockUserManager() {
  delete user_;
}

const User* MockUserManager::GetLoggedInUser() const {
  return user_;
}

User* MockUserManager::GetLoggedInUser() {
  return user_;
}

UserImageManager* MockUserManager::GetUserImageManager() {
  return user_image_manager_.get();
}

// Creates a new User instance.
void MockUserManager::SetLoggedInUser(const std::string& email) {
  delete user_;
  user_ = User::CreateRegularUser(email);
}

UserFlow* MockUserManager::GetCurrentUserFlow() const {
  return user_flow_.get();
}

UserFlow* MockUserManager::GetUserFlow(const std::string&) const {
  return user_flow_.get();
}

ScopedMockUserManagerEnabler::ScopedMockUserManagerEnabler() {
  user_manager_.reset(new MockUserManager());
  old_user_manager_ = UserManager::Set(user_manager_.get());
}

ScopedMockUserManagerEnabler::~ScopedMockUserManagerEnabler() {
  UserManager::Set(old_user_manager_);
}

MockUserManager* ScopedMockUserManagerEnabler::user_manager() {
  return user_manager_.get();
}

}  // namespace chromeos
