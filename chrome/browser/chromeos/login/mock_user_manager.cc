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

const User* MockUserManager::GetActiveUser() const {
  return user_;
}

User* MockUserManager::GetActiveUser() {
  return user_;
}

UserImageManager* MockUserManager::GetUserImageManager() {
  return user_image_manager_.get();
}

// Creates a new User instance.
void MockUserManager::SetActiveUser(const std::string& email) {
  delete user_;
  user_ = User::CreateRegularUser(email);
}

UserFlow* MockUserManager::GetCurrentUserFlow() const {
  return user_flow_.get();
}

UserFlow* MockUserManager::GetUserFlow(const std::string&) const {
  return user_flow_.get();
}

User* MockUserManager::CreatePublicAccountUser(const std::string& email) {
  delete user_;
  user_ = User::CreatePublicAccountUser(email);
  return user_;
}

}  // namespace chromeos
