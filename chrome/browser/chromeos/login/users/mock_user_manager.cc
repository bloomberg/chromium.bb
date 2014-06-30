// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/mock_user_manager.h"

#include "chrome/browser/chromeos/login/users/fake_supervised_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"

namespace chromeos {

MockUserManager::MockUserManager()
    : user_flow_(new DefaultUserFlow()),
      supervised_user_manager_(new FakeSupervisedUserManager()) {
  ProfileHelper::SetProfileToUserForTestingEnabled(true);
}

MockUserManager::~MockUserManager() {
  ProfileHelper::SetProfileToUserForTestingEnabled(false);
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

MultiProfileUserController* MockUserManager::GetMultiProfileUserController() {
  return NULL;
}

UserImageManager* MockUserManager::GetUserImageManager(
    const std::string& user_id) {
  return NULL;
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
  User* user = User::CreatePublicAccountUser(email);
  user_list_.push_back(user);
  ProfileHelper::Get()->SetProfileToUserMappingForTesting(user);
  return user_list_.back();
}

void MockUserManager::AddUser(const std::string& email) {
  User* user = User::CreateRegularUser(email);
  user_list_.push_back(user);
  ProfileHelper::Get()->SetProfileToUserMappingForTesting(user);
}

void MockUserManager::ClearUserList() {
  // Can't use STLDeleteElements because of the protected destructor of User.
  UserList::iterator user;
  for (user = user_list_.begin(); user != user_list_.end(); ++user)
    delete *user;
  user_list_.clear();
}

}  // namespace chromeos
