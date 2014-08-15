// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/mock_user_manager.h"

#include "base/task_runner.h"
#include "chrome/browser/chromeos/login/users/fake_supervised_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"

namespace {

class FakeTaskRunner : public base::TaskRunner {
 public:
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) OVERRIDE {
    task.Run();
    return true;
  }
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE { return true; }

 protected:
  virtual ~FakeTaskRunner() {}
};

}  // namespace

namespace chromeos {

MockUserManager::MockUserManager()
    : ChromeUserManager(new FakeTaskRunner(), new FakeTaskRunner()),
      user_flow_(new DefaultUserFlow()),
      supervised_user_manager_(new FakeSupervisedUserManager()) {
  ProfileHelper::SetProfileToUserForTestingEnabled(true);
}

MockUserManager::~MockUserManager() {
  ProfileHelper::SetProfileToUserForTestingEnabled(false);
  ClearUserList();
}

const user_manager::UserList& MockUserManager::GetUsers() const {
  return user_list_;
}

const user_manager::User* MockUserManager::GetLoggedInUser() const {
  return user_list_.empty() ? NULL : user_list_.front();
}

user_manager::User* MockUserManager::GetLoggedInUser() {
  return user_list_.empty() ? NULL : user_list_.front();
}

user_manager::UserList MockUserManager::GetUnlockUsers() const {
  return user_list_;
}

const std::string& MockUserManager::GetOwnerEmail() const {
  return GetLoggedInUser()->email();
}

const user_manager::User* MockUserManager::GetActiveUser() const {
  return GetLoggedInUser();
}

user_manager::User* MockUserManager::GetActiveUser() {
  return GetLoggedInUser();
}

const user_manager::User* MockUserManager::GetPrimaryUser() const {
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

user_manager::User* MockUserManager::CreatePublicAccountUser(
    const std::string& email) {
  ClearUserList();
  user_manager::User* user = user_manager::User::CreatePublicAccountUser(email);
  user_list_.push_back(user);
  ProfileHelper::Get()->SetProfileToUserMappingForTesting(user);
  return user_list_.back();
}

void MockUserManager::AddUser(const std::string& email) {
  user_manager::User* user = user_manager::User::CreateRegularUser(email);
  user_list_.push_back(user);
  ProfileHelper::Get()->SetProfileToUserMappingForTesting(user);
}

void MockUserManager::ClearUserList() {
  // Can't use STLDeleteElements because of the protected destructor of User.
  user_manager::UserList::iterator user;
  for (user = user_list_.begin(); user != user_list_.end(); ++user)
    delete *user;
  user_list_.clear();
}

}  // namespace chromeos
