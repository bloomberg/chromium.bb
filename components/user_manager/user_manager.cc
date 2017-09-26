// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_manager.h"

#include "base/logging.h"
#include "components/signin/core/account_id/account_id.h"

namespace user_manager {

UserManager* UserManager::instance = nullptr;

UserManager::Observer::~Observer() = default;

void UserManager::Observer::LocalStateChanged(UserManager* user_manager) {}

void UserManager::Observer::OnUserImageChanged(const User& user) {}

void UserManager::Observer::OnUserProfileImageUpdateFailed(const User& user) {}

void UserManager::Observer::OnUserProfileImageUpdated(
    const User& user,
    const gfx::ImageSkia& profile_image) {}

void UserManager::Observer::OnChildStatusChanged(const User& user) {}

void UserManager::UserSessionStateObserver::ActiveUserChanged(
    const User* active_user) {
}

void UserManager::UserSessionStateObserver::UserAddedToSession(
    const User* active_user) {
}

void UserManager::UserSessionStateObserver::ActiveUserHashChanged(
    const std::string& hash) {
}

UserManager::UserSessionStateObserver::~UserSessionStateObserver() {
}

UserManager::UserAccountData::UserAccountData(
    const base::string16& display_name,
    const base::string16& given_name,
    const std::string& locale)
    : display_name_(display_name), given_name_(given_name), locale_(locale) {
}

UserManager::UserAccountData::~UserAccountData() {
}

void UserManager::Initialize() {
  DCHECK(!UserManager::instance);
  UserManager::SetInstance(this);
}

// static
bool UserManager::IsInitialized() {
  return UserManager::instance;
}

void UserManager::Destroy() {
  DCHECK(UserManager::instance == this);
  UserManager::SetInstance(nullptr);
}

// static
UserManager* user_manager::UserManager::Get() {
  CHECK(UserManager::instance);
  return UserManager::instance;
}

UserManager::~UserManager() {
}

// static
void UserManager::SetInstance(UserManager* user_manager) {
  UserManager::instance = user_manager;
}

// static
UserManager* user_manager::UserManager::GetForTesting() {
  return UserManager::instance;
}

// static
UserManager* UserManager::SetForTesting(UserManager* user_manager) {
  UserManager* previous_instance = UserManager::instance;
  UserManager::instance = user_manager;
  return previous_instance;
}

ScopedUserSessionStateObserver::ScopedUserSessionStateObserver(
    UserManager::UserSessionStateObserver* observer)
    : observer_(observer) {
  if (UserManager::IsInitialized())
    UserManager::Get()->AddSessionStateObserver(observer_);
}

ScopedUserSessionStateObserver::~ScopedUserSessionStateObserver() {
  if (UserManager::IsInitialized())
    UserManager::Get()->RemoveSessionStateObserver(observer_);
}

}  // namespace user_manager
