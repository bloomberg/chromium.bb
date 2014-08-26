// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/fake_user_manager.h"

#include "base/task_runner.h"
#include "chrome/browser/chromeos/login/users/fake_supervised_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/grit/theme_resources.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_type.h"
#include "ui/base/resource/resource_bundle.h"

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

FakeUserManager::FakeUserManager()
    : ChromeUserManager(new FakeTaskRunner(), new FakeTaskRunner()),
      supervised_user_manager_(new FakeSupervisedUserManager),
      primary_user_(NULL),
      multi_profile_user_controller_(NULL) {
}

FakeUserManager::~FakeUserManager() {
  // Can't use STLDeleteElements because of the private destructor of User.
  for (user_manager::UserList::iterator it = user_list_.begin();
       it != user_list_.end();
       it = user_list_.erase(it)) {
    delete *it;
  }
}

const user_manager::User* FakeUserManager::AddUser(const std::string& email) {
  user_manager::User* user = user_manager::User::CreateRegularUser(email);
  user->set_username_hash(
      ProfileHelper::GetUserIdHashByUserIdForTesting(email));
  user->SetStubImage(user_manager::UserImage(
                         *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                             IDR_PROFILE_PICTURE_LOADING)),
                     user_manager::User::USER_IMAGE_PROFILE,
                     false);
  user_list_.push_back(user);
  return user;
}

const user_manager::User* FakeUserManager::AddPublicAccountUser(
    const std::string& email) {
  user_manager::User* user = user_manager::User::CreatePublicAccountUser(email);
  user->set_username_hash(
      ProfileHelper::GetUserIdHashByUserIdForTesting(email));
  user->SetStubImage(user_manager::UserImage(
                         *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                             IDR_PROFILE_PICTURE_LOADING)),
                     user_manager::User::USER_IMAGE_PROFILE,
                     false);
  user_list_.push_back(user);
  return user;
}

void FakeUserManager::AddKioskAppUser(const std::string& kiosk_app_username) {
  user_manager::User* user =
      user_manager::User::CreateKioskAppUser(kiosk_app_username);
  user->set_username_hash(
      ProfileHelper::GetUserIdHashByUserIdForTesting(kiosk_app_username));
  user_list_.push_back(user);
}

void FakeUserManager::RemoveUserFromList(const std::string& email) {
  user_manager::UserList::iterator it = user_list_.begin();
  while (it != user_list_.end() && (*it)->email() != email) ++it;
  if (it != user_list_.end()) {
    delete *it;
    user_list_.erase(it);
  }
}

void FakeUserManager::LoginUser(const std::string& email) {
  UserLoggedIn(
      email, ProfileHelper::GetUserIdHashByUserIdForTesting(email), false);
}

const user_manager::UserList& FakeUserManager::GetUsers() const {
  return user_list_;
}

user_manager::UserList FakeUserManager::GetUsersAdmittedForMultiProfile()
    const {
  user_manager::UserList result;
  for (user_manager::UserList::const_iterator it = user_list_.begin();
       it != user_list_.end();
       ++it) {
    if ((*it)->GetType() == user_manager::USER_TYPE_REGULAR &&
        !(*it)->is_logged_in())
      result.push_back(*it);
  }
  return result;
}

const user_manager::UserList& FakeUserManager::GetLoggedInUsers() const {
  return logged_in_users_;
}

void FakeUserManager::UserLoggedIn(const std::string& email,
                                   const std::string& username_hash,
                                   bool browser_restart) {
  for (user_manager::UserList::const_iterator it = user_list_.begin();
       it != user_list_.end();
       ++it) {
    if ((*it)->username_hash() == username_hash) {
      (*it)->set_is_logged_in(true);
      (*it)->set_profile_is_created();
      logged_in_users_.push_back(*it);

      if (!primary_user_)
        primary_user_ = *it;
      break;
    }
  }
}

user_manager::User* FakeUserManager::GetActiveUserInternal() const {
  if (user_list_.size()) {
    if (!active_user_id_.empty()) {
      for (user_manager::UserList::const_iterator it = user_list_.begin();
           it != user_list_.end();
           ++it) {
        if ((*it)->email() == active_user_id_)
          return *it;
      }
    }
    return user_list_[0];
  }
  return NULL;
}

const user_manager::User* FakeUserManager::GetActiveUser() const {
  return GetActiveUserInternal();
}

user_manager::User* FakeUserManager::GetActiveUser() {
  return GetActiveUserInternal();
}

void FakeUserManager::SwitchActiveUser(const std::string& email) {
  active_user_id_ = email;
  ProfileHelper::Get()->ActiveUserHashChanged(
      ProfileHelper::GetUserIdHashByUserIdForTesting(email));
}

void FakeUserManager::SaveUserDisplayName(
    const std::string& username,
    const base::string16& display_name) {
  for (user_manager::UserList::iterator it = user_list_.begin();
       it != user_list_.end();
       ++it) {
    if ((*it)->email() == username) {
      (*it)->set_display_name(display_name);
      return;
    }
  }
}

MultiProfileUserController* FakeUserManager::GetMultiProfileUserController() {
  return multi_profile_user_controller_;
}

SupervisedUserManager* FakeUserManager::GetSupervisedUserManager() {
  return supervised_user_manager_.get();
}

UserImageManager* FakeUserManager::GetUserImageManager(
    const std::string& /* user_id */) {
  return NULL;
}

const user_manager::UserList& FakeUserManager::GetLRULoggedInUsers() const {
  return user_list_;
}

user_manager::UserList FakeUserManager::GetUnlockUsers() const {
  return user_list_;
}

const std::string& FakeUserManager::GetOwnerEmail() const {
  return owner_email_;
}

bool FakeUserManager::IsKnownUser(const std::string& email) const {
  return true;
}

const user_manager::User* FakeUserManager::FindUser(
    const std::string& email) const {
  const user_manager::UserList& users = GetUsers();
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end();
       ++it) {
    if ((*it)->email() == email)
      return *it;
  }
  return NULL;
}

user_manager::User* FakeUserManager::FindUserAndModify(
    const std::string& email) {
  return NULL;
}

const user_manager::User* FakeUserManager::GetLoggedInUser() const {
  return NULL;
}

user_manager::User* FakeUserManager::GetLoggedInUser() {
  return NULL;
}

const user_manager::User* FakeUserManager::GetPrimaryUser() const {
  return primary_user_;
}

base::string16 FakeUserManager::GetUserDisplayName(
    const std::string& username) const {
  return base::string16();
}

std::string FakeUserManager::GetUserDisplayEmail(
    const std::string& username) const {
  return std::string();
}

bool FakeUserManager::IsCurrentUserOwner() const {
  return false;
}

bool FakeUserManager::IsCurrentUserNew() const {
  return false;
}

bool FakeUserManager::IsCurrentUserNonCryptohomeDataEphemeral() const {
  return false;
}

bool FakeUserManager::CanCurrentUserLock() const {
  return false;
}

bool FakeUserManager::IsUserLoggedIn() const {
  return logged_in_users_.size() > 0;
}

bool FakeUserManager::IsLoggedInAsRegularUser() const {
  return true;
}

bool FakeUserManager::IsLoggedInAsDemoUser() const {
  return false;
}

bool FakeUserManager::IsLoggedInAsPublicAccount() const {
  return false;
}

bool FakeUserManager::IsLoggedInAsGuest() const {
  return false;
}

bool FakeUserManager::IsLoggedInAsSupervisedUser() const {
  return false;
}

bool FakeUserManager::IsLoggedInAsKioskApp() const {
  const user_manager::User* active_user = GetActiveUser();
  return active_user
             ? active_user->GetType() == user_manager::USER_TYPE_KIOSK_APP
             : false;
}

bool FakeUserManager::IsLoggedInAsStub() const {
  return false;
}

bool FakeUserManager::IsSessionStarted() const {
  return false;
}

bool FakeUserManager::IsUserNonCryptohomeDataEphemeral(
    const std::string& email) const {
  return false;
}

UserFlow* FakeUserManager::GetCurrentUserFlow() const {
  return NULL;
}

UserFlow* FakeUserManager::GetUserFlow(const std::string& email) const {
  return NULL;
}

bool FakeUserManager::AreSupervisedUsersAllowed() const {
  return true;
}

bool FakeUserManager::AreEphemeralUsersEnabled() const {
  return false;
}

const std::string& FakeUserManager::GetApplicationLocale() const {
  static const std::string default_locale("en-US");
  return default_locale;
}

PrefService* FakeUserManager::GetLocalState() const {
  return NULL;
}

bool FakeUserManager::IsEnterpriseManaged() const {
  return false;
}

bool FakeUserManager::IsDemoApp(const std::string& user_id) const {
  return false;
}

bool FakeUserManager::IsKioskApp(const std::string& user_id) const {
  return false;
}

bool FakeUserManager::IsPublicAccountMarkedForRemoval(
    const std::string& user_id) const {
  return false;
}

}  // namespace chromeos
