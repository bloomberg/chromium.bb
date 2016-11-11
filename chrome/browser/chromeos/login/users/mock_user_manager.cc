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
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override {
    task.Run();
    return true;
  }
  bool RunsTasksOnCurrentThread() const override { return true; }

 protected:
  ~FakeTaskRunner() override {}
};

}  // namespace

namespace chromeos {

MockUserManager::MockUserManager()
    : ChromeUserManager(new FakeTaskRunner()),
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

user_manager::UserList MockUserManager::GetUnlockUsers() const {
  return user_list_;
}

const AccountId& MockUserManager::GetOwnerAccountId() const {
  return GetActiveUser()->GetAccountId();
}

const user_manager::User* MockUserManager::GetActiveUser() const {
  return user_list_.empty() ? nullptr : user_list_.front();
}

user_manager::User* MockUserManager::GetActiveUser() {
  return user_list_.empty() ? nullptr : user_list_.front();
}

const user_manager::User* MockUserManager::GetPrimaryUser() const {
  return GetActiveUser();
}

BootstrapManager* MockUserManager::GetBootstrapManager() {
  return nullptr;
}

MultiProfileUserController* MockUserManager::GetMultiProfileUserController() {
  return nullptr;
}

UserImageManager* MockUserManager::GetUserImageManager(
    const AccountId& account_id) {
  return nullptr;
}

SupervisedUserManager* MockUserManager::GetSupervisedUserManager() {
  return supervised_user_manager_.get();
}

// Creates a new User instance.
void MockUserManager::SetActiveUser(const AccountId& account_id) {
  ClearUserList();
  AddUser(account_id);
}

UserFlow* MockUserManager::GetCurrentUserFlow() const {
  return user_flow_.get();
}

UserFlow* MockUserManager::GetUserFlow(const AccountId&) const {
  return user_flow_.get();
}

user_manager::User* MockUserManager::CreatePublicAccountUser(
    const AccountId& account_id) {
  ClearUserList();
  user_manager::User* user =
      user_manager::User::CreatePublicAccountUser(account_id);
  user_list_.push_back(user);
  ProfileHelper::Get()->SetProfileToUserMappingForTesting(user);
  return user_list_.back();
}

user_manager::User* MockUserManager::CreateKioskAppUser(
    const AccountId& account_id) {
  ClearUserList();
  user_list_.push_back(user_manager::User::CreateKioskAppUser(account_id));
  ProfileHelper::Get()->SetProfileToUserMappingForTesting(user_list_.back());
  return user_list_.back();
}

void MockUserManager::AddUser(const AccountId& account_id) {
  AddUserWithAffiliation(account_id, false);
}

void MockUserManager::AddUserWithAffiliation(const AccountId& account_id,
                                             bool is_affiliated) {
  user_manager::User* user = user_manager::User::CreateRegularUser(account_id);
  user->SetAffiliation(is_affiliated);
  user_list_.push_back(user);
  ProfileHelper::Get()->SetProfileToUserMappingForTesting(user);
}

void MockUserManager::ClearUserList() {
  // Can't use STLDeleteElements because of the protected destructor of User.
  for (user_manager::UserList::iterator user = user_list_.begin();
       user != user_list_.end(); ++user)
    delete *user;
  user_list_.clear();
}

bool MockUserManager::ShouldReportUser(const std::string& user_id) const {
  for (auto* user : user_list_) {
    if (user->GetAccountId().GetUserEmail() == user_id)
      return user->IsAffiliated();
  }
  NOTREACHED();
  return false;
}

}  // namespace chromeos
