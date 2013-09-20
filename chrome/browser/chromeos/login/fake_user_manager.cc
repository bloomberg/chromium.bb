// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/fake_user_manager.h"

namespace {

// As defined in /chromeos/dbus/cryptohome_client.cc.
static const char kUserIdHashSuffix[] = "-hash";

}  // namespace

namespace chromeos {

FakeUserManager::FakeUserManager() {}
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
  user_list_.push_back(user);
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
      break;
    }
  }
}

const User* FakeUserManager::GetActiveUser() const {
  return GetActiveUser();
}

User* FakeUserManager::GetActiveUser() {
  // Just return the first user.
  if (user_list_.size())
    return user_list_[0];
  return NULL;
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

const User* FakeUserManager::CreateLocallyManagedUserRecord(
    const std::string& manager_id,
    const std::string& local_user_id,
    const std::string& sync_user_id,
    const string16& display_name) {
  return NULL;
}

std::string FakeUserManager::GenerateUniqueLocallyManagedUserId() {
  return std::string();
}

bool FakeUserManager::IsKnownUser(const std::string& email) const {
  return true;
}

const User* FakeUserManager::FindUser(const std::string& email) const {
  return NULL;
}

const User* FakeUserManager::FindLocallyManagedUser(
    const string16& display_name) const {
  return NULL;
}

const User* FakeUserManager::GetLoggedInUser() const {
  return NULL;
}

User* FakeUserManager::GetLoggedInUser() {
  return NULL;
}

const User* FakeUserManager::GetPrimaryUser() const {
  return NULL;
}

string16 FakeUserManager::GetUserDisplayName(
    const std::string& username) const {
  return string16();
}

std::string FakeUserManager::GetUserDisplayEmail(
    const std::string& username) const {
  return std::string();
}

std::string FakeUserManager::GetManagedUserSyncId(
    const std::string& managed_user_id) const {
  return std::string();
}

string16 FakeUserManager::GetManagerDisplayNameForManagedUser(
    const std::string& managed_user_id) const {
  return string16();
}

std::string FakeUserManager::GetManagerUserIdForManagedUser(
    const std::string& managed_user_id) const {
  return std::string();
}

std::string FakeUserManager::GetManagerDisplayEmailForManagedUser(
    const std::string& managed_user_id) const {
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
  return true;
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
  return false;
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

}  // namespace chromeos
