// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/fake_user_manager.h"

#include "chrome/browser/chromeos/login/fake_supervised_user_manager.h"

namespace {

// As defined in /chromeos/dbus/cryptohome_client.cc.
static const char kUserIdHashSuffix[] = "-hash";

}  // namespace

namespace chromeos {

FakeUserManager::FakeUserManager()
    : supervised_user_manager_(new FakeSupervisedUserManager),
      primary_user_(NULL) {}

FakeUserManager::~FakeUserManager() {
  // Can't use STLDeleteElements because of the private destructor of User.
  for (UserList::iterator it = user_list_.begin(); it != user_list_.end();
       it = user_list_.erase(it)) {
    delete *it;
  }
}

void FakeUserManager::AddUser(const std::string& email) {
  User* user = User::CreateRegularUser(email);
  user->set_username_hash(email + kUserIdHashSuffix);
  user->SetStubImage(User::kProfileImageIndex, false);
  user_list_.push_back(user);
}

void FakeUserManager::AddKioskAppUser(const std::string& kiosk_app_username) {
  User* user = User::CreateKioskAppUser(kiosk_app_username);
  user->set_username_hash(kiosk_app_username + kUserIdHashSuffix);
  user_list_.push_back(user);
}

void FakeUserManager::LoginUser(const std::string& email) {
  UserLoggedIn(email, email + kUserIdHashSuffix, false);
}

const UserList& FakeUserManager::GetUsers() const {
  return user_list_;
}

UserList FakeUserManager::GetUsersAdmittedForMultiProfile() const {
  UserList result;
  for (UserList::const_iterator it = user_list_.begin();
       it != user_list_.end();
       ++it) {
    if ((*it)->GetType() == User::USER_TYPE_REGULAR && !(*it)->is_logged_in())
      result.push_back(*it);
  }
  return result;
}

const UserList& FakeUserManager::GetLoggedInUsers() const {
  return logged_in_users_;
}

void FakeUserManager::UserLoggedIn(const std::string& email,
                                   const std::string& username_hash,
                                   bool browser_restart) {
  for (UserList::const_iterator it = user_list_.begin();
       it != user_list_.end();
       ++it) {
    if ((*it)->username_hash() == username_hash) {
      (*it)->set_is_logged_in(true);
      logged_in_users_.push_back(*it);

      if (!primary_user_)
        primary_user_ = *it;
      break;
    }
  }
}

User* FakeUserManager::GetActiveUserInternal() const {
  if (user_list_.size())
    return user_list_[0];
  return NULL;
}

const User* FakeUserManager::GetActiveUser() const {
  return GetActiveUserInternal();
}

User* FakeUserManager::GetActiveUser() {
  return GetActiveUserInternal();
}

void FakeUserManager::SaveUserDisplayName(
    const std::string& username,
    const string16& display_name) {
  for (UserList::iterator it = user_list_.begin();
       it != user_list_.end(); ++it) {
    if ((*it)->email() == username) {
      (*it)->set_display_name(display_name);
      return;
    }
  }
}

void FakeUserManager::UpdateUserAccountData(const std::string&, const string16&,
                           const std::string&) {
  // Not implemented
}

SupervisedUserManager* FakeUserManager::GetSupervisedUserManager() {
  return supervised_user_manager_.get();
}

UserImageManager* FakeUserManager::GetUserImageManager() {
  return NULL;
}

const UserList& FakeUserManager::GetLRULoggedInUsers() {
  return user_list_;
}

UserList FakeUserManager::GetUnlockUsers() const {
  return user_list_;
}

const std::string& FakeUserManager::GetOwnerEmail() {
  return owner_email_;
}

bool FakeUserManager::IsKnownUser(const std::string& email) const {
  return true;
}

const User* FakeUserManager::FindUser(const std::string& email) const {
  return NULL;
}

const User* FakeUserManager::GetLoggedInUser() const {
  return NULL;
}

User* FakeUserManager::GetLoggedInUser() {
  return NULL;
}

const User* FakeUserManager::GetPrimaryUser() const {
  return primary_user_;
}

User* FakeUserManager::GetUserByProfile(Profile* profile) const {
  return primary_user_;
}

string16 FakeUserManager::GetUserDisplayName(
    const std::string& username) const {
  return string16();
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

bool FakeUserManager::IsLoggedInAsLocallyManagedUser() const {
  return false;
}

bool FakeUserManager::IsLoggedInAsKioskApp() const {
  const User* active_user = GetActiveUser();
  return active_user ?
      active_user->GetType() == User::USER_TYPE_KIOSK_APP :
      false;
}

bool FakeUserManager::IsLoggedInAsStub() const {
  return false;
}

bool FakeUserManager::IsSessionStarted() const {
  return false;
}

bool FakeUserManager::UserSessionsRestored() const {
  return false;
}

bool FakeUserManager::HasBrowserRestarted() const {
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

bool FakeUserManager::GetAppModeChromeClientOAuthInfo(
    std::string* chrome_client_id,
    std::string* chrome_client_secret) {
  return false;
}

bool FakeUserManager::AreLocallyManagedUsersAllowed() const {
  return true;
}

base::FilePath FakeUserManager::GetUserProfileDir(
    const std::string&email) const {
  return base::FilePath();
}

void FakeUserManager::RespectLocalePreference(Profile* profile,
                                              const User* user) const {
}

}  // namespace chromeos
