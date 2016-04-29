// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"
#include "chrome/browser/chromeos/login/users/fake_supervised_user_manager.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/login_state.h"
#include "chromeos/login/user_names.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_type.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

class FakeSupervisedUserManager;

FakeChromeUserManager::FakeChromeUserManager()
    : supervised_user_manager_(new FakeSupervisedUserManager),
      bootstrap_manager_(NULL),
      multi_profile_user_controller_(NULL) {
  ProfileHelper::SetProfileToUserForTestingEnabled(true);
}

FakeChromeUserManager::~FakeChromeUserManager() {
  ProfileHelper::SetProfileToUserForTestingEnabled(false);
}

const user_manager::User* FakeChromeUserManager::AddUser(
    const AccountId& account_id) {
  return AddUserWithAffiliation(account_id, false);
}

const user_manager::User* FakeChromeUserManager::AddUserWithAffiliation(
    const AccountId& account_id,
    bool is_affiliated) {
  user_manager::User* user = user_manager::User::CreateRegularUser(account_id);
  user->SetAffiliation(is_affiliated);
  user->set_username_hash(ProfileHelper::GetUserIdHashByUserIdForTesting(
      account_id.GetUserEmail()));
  user->SetStubImage(base::WrapUnique(new user_manager::UserImage(
                         *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                             IDR_PROFILE_PICTURE_LOADING))),
                     user_manager::User::USER_IMAGE_PROFILE, false);
  users_.push_back(user);
  chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(user);
  return user;
}

user_manager::User* FakeChromeUserManager::AddKioskAppUser(
    const AccountId& account_id) {
  user_manager::User* user = user_manager::User::CreateKioskAppUser(account_id);
  user->set_username_hash(ProfileHelper::GetUserIdHashByUserIdForTesting(
      account_id.GetUserEmail()));
  users_.push_back(user);
  return user;
}

const user_manager::User* FakeChromeUserManager::AddPublicAccountUser(
    const AccountId& account_id) {
  user_manager::User* user =
      user_manager::User::CreatePublicAccountUser(account_id);
  user->set_username_hash(ProfileHelper::GetUserIdHashByUserIdForTesting(
      account_id.GetUserEmail()));
  user->SetStubImage(base::WrapUnique(new user_manager::UserImage(
                         *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                             IDR_PROFILE_PICTURE_LOADING))),
                     user_manager::User::USER_IMAGE_PROFILE, false);
  users_.push_back(user);
  chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(user);
  return user;
}

void FakeChromeUserManager::LoginUser(const AccountId& account_id) {
  UserLoggedIn(account_id, ProfileHelper::GetUserIdHashByUserIdForTesting(
                               account_id.GetUserEmail()),
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
    const AccountId& /* account_id */) {
  return nullptr;
}

void FakeChromeUserManager::SetUserFlow(const AccountId& account_id,
                                        UserFlow* flow) {
  ResetUserFlow(account_id);
  specific_flows_[account_id] = flow;
}

UserFlow* FakeChromeUserManager::GetCurrentUserFlow() const {
  if (!IsUserLoggedIn())
    return GetDefaultUserFlow();
  return GetUserFlow(GetLoggedInUser()->GetAccountId());
}

UserFlow* FakeChromeUserManager::GetUserFlow(
    const AccountId& account_id) const {
  FlowMap::const_iterator it = specific_flows_.find(account_id);
  if (it != specific_flows_.end())
    return it->second;
  return GetDefaultUserFlow();
}

void FakeChromeUserManager::ResetUserFlow(const AccountId& account_id) {
  FlowMap::iterator it = specific_flows_.find(account_id);
  if (it != specific_flows_.end()) {
    delete it->second;
    specific_flows_.erase(it);
  }
}

void FakeChromeUserManager::SwitchActiveUser(const AccountId& account_id) {
  active_account_id_ = account_id;
  ProfileHelper::Get()->ActiveUserHashChanged(
      ProfileHelper::GetUserIdHashByUserIdForTesting(
          account_id.GetUserEmail()));
  active_user_ = nullptr;
  if (!users_.empty() && active_account_id_.is_valid()) {
    for (user_manager::User* const user : users_) {
      if (user->GetAccountId() == active_account_id_) {
        active_user_ = user;
        user->set_is_active(true);
      } else {
        user->set_is_active(false);
      }
    }
  }
}

const AccountId& FakeChromeUserManager::GetOwnerAccountId() const {
  return owner_account_id_;
}

void FakeChromeUserManager::SessionStarted() {
}

void FakeChromeUserManager::RemoveUser(
    const AccountId& account_id,
    user_manager::RemoveUserDelegate* delegate) {}

void FakeChromeUserManager::RemoveUserFromList(const AccountId& account_id) {
  WallpaperManager::Get()->RemoveUserWallpaperInfo(account_id);
  FakeUserManager::RemoveUserFromList(account_id);
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

void FakeChromeUserManager::UpdateLoginState(
    const user_manager::User* active_user,
    const user_manager::User* primary_user,
    bool is_current_user_owner) const {
  chrome_user_manager_util::UpdateLoginState(active_user, primary_user,
                                             is_current_user_owner);
}

bool FakeChromeUserManager::GetPlatformKnownUserId(
    const std::string& user_email,
    const std::string& gaia_id,
    AccountId* out_account_id) const {
  return chrome_user_manager_util::GetPlatformKnownUserId(user_email, gaia_id,
                                                          out_account_id);
}

const AccountId& FakeChromeUserManager::GetGuestAccountId() const {
  return login::GuestAccountId();
}

bool FakeChromeUserManager::IsFirstExecAfterBoot() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kFirstExecAfterBoot);
}

void FakeChromeUserManager::AsyncRemoveCryptohome(
    const AccountId& account_id) const {
  NOTIMPLEMENTED();
}

bool FakeChromeUserManager::IsGuestAccountId(
    const AccountId& account_id) const {
  return account_id == login::GuestAccountId();
}

bool FakeChromeUserManager::IsStubAccountId(const AccountId& account_id) const {
  return account_id == login::StubAccountId();
}

bool FakeChromeUserManager::IsSupervisedAccountId(
    const AccountId& account_id) const {
  return gaia::ExtractDomainName(account_id.GetUserEmail()) ==
         chromeos::login::kSupervisedUserDomain;
}

bool FakeChromeUserManager::HasBrowserRestarted() const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return base::SysInfo::IsRunningOnChromeOS() &&
         command_line->HasSwitch(chromeos::switches::kLoginUser);
}

const gfx::ImageSkia& FakeChromeUserManager::GetResourceImagekiaNamed(
    int id) const {
  return *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(id);
}

base::string16 FakeChromeUserManager::GetResourceStringUTF16(
    int string_id) const {
  return base::string16();
}

void FakeChromeUserManager::ScheduleResolveLocale(
    const std::string& locale,
    const base::Closure& on_resolved_callback,
    std::string* out_resolved_locale) const {
  NOTIMPLEMENTED();
  return;
}

bool FakeChromeUserManager::IsValidDefaultUserImageId(int image_index) const {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace chromeos
