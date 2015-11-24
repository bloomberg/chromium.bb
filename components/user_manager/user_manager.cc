// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_manager.h"

#include "base/logging.h"
#include "chromeos/login/user_names.h"
#include "components/signin/core/account_id/account_id.h"

namespace user_manager {

UserManager* UserManager::instance = NULL;

UserManager::Observer::~Observer() {
}

void UserManager::Observer::LocalStateChanged(UserManager* user_manager) {
}

void UserManager::UserSessionStateObserver::ActiveUserChanged(
    const User* active_user) {
}

void UserManager::UserSessionStateObserver::UserAddedToSession(
    const User* active_user) {
}

void UserManager::UserSessionStateObserver::ActiveUserHashChanged(
    const std::string& hash) {
}

void UserManager::UserSessionStateObserver::UserChangedChildStatus(User* user) {
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
  UserManager::SetInstance(NULL);
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

// static
AccountId UserManager::GetKnownUserAccountId(const std::string& user_email,
                                             const std::string& gaia_id) {
  // In tests empty accounts are possible.
  if (user_email.empty() && gaia_id.empty())
    return EmptyAccountId();

  if (user_email == chromeos::login::kStubUser)
    return chromeos::login::StubAccountId();

  if (user_email == chromeos::login::kGuestUserName)
    return chromeos::login::GuestAccountId();

  UserManager* user_manager = Get();
  if (user_manager)
    return user_manager->GetKnownUserAccountIdImpl(user_email, gaia_id);

  // This is fallback for tests.
  return (gaia_id.empty()
              ? AccountId::FromUserEmail(user_email)
              : AccountId::FromUserEmailGaiaId(user_email, gaia_id));
}

}  // namespace user_manager
