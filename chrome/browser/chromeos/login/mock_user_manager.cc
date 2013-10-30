// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_user_manager.h"

#include "chrome/browser/chromeos/login/fake_supervised_user_manager.h"

namespace chromeos {

MockUserManager::MockUserManager()
    : user_flow_(new DefaultUserFlow()),
      supervised_user_manager_(new FakeSupervisedUserManager()) {}

MockUserManager::~MockUserManager() {
  ClearUserList();
}

const UserList& MockUserManager::GetUsers() const {
  return user_list_;
}

const User* MockUserManager::GetLoggedInUser() const {
  return user_list_.empty() ? NULL : user_list_.front();
}

User* MockUserManager::GetLoggedInUser() {
  return user_list_.empty() ? NULL : user_list_.front();
}

UserList MockUserManager::GetUnlockUsers() const {
  return user_list_;
}

const std::string& MockUserManager::GetOwnerEmail() {
  return GetLoggedInUser()->email();
}

const User* MockUserManager::GetActiveUser() const {
  return GetLoggedInUser();
}

User* MockUserManager::GetActiveUser() {
  return GetLoggedInUser();
}

const User* MockUserManager::GetPrimaryUser() const {
  return GetLoggedInUser();
}

User* MockUserManager::GetUserByProfile(Profile* profile) const {
  return user_list_.empty() ? NULL : user_list_.front();
}

UserImageManager* MockUserManager::GetUserImageManager() {
  return user_image_manager_.get();
}

SupervisedUserManager* MockUserManager::GetSupervisedUserManager() {
  return supervised_user_manager_.get();
}

// Creates a new User instance.
void MockUserManager::SetActiveUser(const std::string& email) {
  ClearUserList();
  AddUser(email);
}

UserFlow* MockUserManager::GetCurrentUserFlow() const {
  return user_flow_.get();
}

UserFlow* MockUserManager::GetUserFlow(const std::string&) const {
  return user_flow_.get();
}

User* MockUserManager::CreatePublicAccountUser(const std::string& email) {
  ClearUserList();
  user_list_.push_back(User::CreatePublicAccountUser(email));
  return user_list_.back();
}

void MockUserManager::AddUser(const std::string& email) {
  user_list_.push_back(User::CreateRegularUser(email));
}

void MockUserManager::ClearUserList() {
  // Can't use STLDeleteElements because of the protected destructor of User.
  UserList::iterator user;
  for (user = user_list_.begin(); user != user_list_.end(); ++user)
    delete *user;
  user_list_.clear();
}

void MockUserManager::RespectLocalePreference(Profile* profile,
                                              const User* user) const {
}

}  // namespace chromeos
