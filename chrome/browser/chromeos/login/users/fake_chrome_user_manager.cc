// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"

#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/fake_supervised_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/grit/theme_resources.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_type.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

class FakeSupervisedUserManager;

FakeChromeUserManager::FakeChromeUserManager()
    : supervised_user_manager_(new FakeSupervisedUserManager),
      bootstrap_manager_(NULL),
      multi_profile_user_controller_(NULL) {
}

FakeChromeUserManager::~FakeChromeUserManager() {
}

const user_manager::User* FakeChromeUserManager::AddUser(
    const std::string& email) {
  user_manager::User* user = user_manager::User::CreateRegularUser(email);
  user->set_username_hash(
      ProfileHelper::GetUserIdHashByUserIdForTesting(email));
  user->SetStubImage(user_manager::UserImage(
                         *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                             IDR_PROFILE_PICTURE_LOADING)),
                     user_manager::User::USER_IMAGE_PROFILE, false);
  users_.push_back(user);
  return user;
}

const user_manager::User* FakeChromeUserManager::AddPublicAccountUser(
    const std::string& email) {
  user_manager::User* user = user_manager::User::CreatePublicAccountUser(email);
  user->set_username_hash(
      ProfileHelper::GetUserIdHashByUserIdForTesting(email));
  user->SetStubImage(user_manager::UserImage(
                         *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                             IDR_PROFILE_PICTURE_LOADING)),
                     user_manager::User::USER_IMAGE_PROFILE, false);
  users_.push_back(user);
  return user;
}

void FakeChromeUserManager::AddKioskAppUser(
    const std::string& kiosk_app_username) {
  user_manager::User* user =
      user_manager::User::CreateKioskAppUser(kiosk_app_username);
  user->set_username_hash(
      ProfileHelper::GetUserIdHashByUserIdForTesting(kiosk_app_username));
  users_.push_back(user);
}

void FakeChromeUserManager::LoginUser(const std::string& email) {
  UserLoggedIn(email, ProfileHelper::GetUserIdHashByUserIdForTesting(email),
               false /* browser_restart */);
}

BootstrapManager* FakeChromeUserManager::GetBootstrapManager() {
  return bootstrap_manager_;
}

MultiProfileUserController*
FakeChromeUserManager::GetMultiProfileUserController() {
  return multi_profile_user_controller_;
}

SupervisedUserManager* FakeChromeUserManager::GetSupervisedUserManager() {
  return supervised_user_manager_.get();
}

UserImageManager* FakeChromeUserManager::GetUserImageManager(
    const std::string& /* user_id */) {
  return nullptr;
}

void FakeChromeUserManager::SetUserFlow(const std::string& email,
                                        UserFlow* flow) {
  ResetUserFlow(email);
  specific_flows_[email] = flow;
}

UserFlow* FakeChromeUserManager::GetCurrentUserFlow() const {
  if (!IsUserLoggedIn())
    return GetDefaultUserFlow();
  return GetUserFlow(GetLoggedInUser()->email());
}

UserFlow* FakeChromeUserManager::GetUserFlow(const std::string& email) const {
  FlowMap::const_iterator it = specific_flows_.find(email);
  if (it != specific_flows_.end())
    return it->second;
  return GetDefaultUserFlow();
}

void FakeChromeUserManager::ResetUserFlow(const std::string& email) {
  FlowMap::iterator it = specific_flows_.find(email);
  if (it != specific_flows_.end()) {
    delete it->second;
    specific_flows_.erase(it);
  }
}

void FakeChromeUserManager::SwitchActiveUser(const std::string& email) {
  active_user_id_ = email;
  ProfileHelper::Get()->ActiveUserHashChanged(
      ProfileHelper::GetUserIdHashByUserIdForTesting(email));
  if (!users_.empty() && !active_user_id_.empty()) {
    for (user_manager::User* user : users_)
      user->set_is_active(user->email() == active_user_id_);
  }
}

const std::string& FakeChromeUserManager::GetOwnerEmail() const {
  return owner_email_;
}

void FakeChromeUserManager::SessionStarted() {
}

void FakeChromeUserManager::RemoveUser(
    const std::string& email,
    user_manager::RemoveUserDelegate* delegate) {
}

user_manager::UserList
FakeChromeUserManager::GetUsersAllowedForSupervisedUsersCreation() const {
  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = true;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  bool supervised_users_allowed = AreSupervisedUsersAllowed();

  // Restricted either by policy or by owner.
  if (!allow_new_user || !supervised_users_allowed)
    return user_manager::UserList();

  return ChromeUserManager::GetUsersAllowedAsSupervisedUserManagers(GetUsers());
}

user_manager::UserList FakeChromeUserManager::GetUsersAllowedForMultiProfile()
    const {
  // Supervised users are not allowed to use multi-profiles.
  if (GetLoggedInUsers().size() == 1 &&
      GetPrimaryUser()->GetType() != user_manager::USER_TYPE_REGULAR) {
    return user_manager::UserList();
  }

  user_manager::UserList result;
  const user_manager::UserList& users = GetUsers();
  for (user_manager::User* user : users) {
    if (user->GetType() == user_manager::USER_TYPE_REGULAR &&
        !user->is_logged_in()) {
      result.push_back(user);
    }
  }

  return result;
}

UserFlow* FakeChromeUserManager::GetDefaultUserFlow() const {
  if (!default_flow_.get())
    default_flow_.reset(new DefaultUserFlow());
  return default_flow_.get();
}

}  // namespace chromeos
