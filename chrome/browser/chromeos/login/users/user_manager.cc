// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/user_manager.h"

#include "base/command_line.h"
#include "base/prefs/pref_registry_simple.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/chrome_switches.h"

namespace chromeos {

static UserManager* g_user_manager = NULL;

UserManager::Observer::~Observer() {
}

void UserManager::Observer::LocalStateChanged(UserManager* user_manager) {
}

void UserManager::UserSessionStateObserver::ActiveUserChanged(
    const user_manager::User* active_user) {
}

void UserManager::UserSessionStateObserver::UserAddedToSession(
    const user_manager::User* active_user) {
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
    : display_name_(display_name),
      given_name_(given_name),
      locale_(locale) {
}

UserManager::UserAccountData::~UserAccountData() {}

// static
void UserManager::Initialize() {
  CHECK(!g_user_manager);
  g_user_manager = new ChromeUserManager();
}

// static
bool UserManager::IsInitialized() {
  return g_user_manager;
}

void UserManager::Destroy() {
  DCHECK(g_user_manager);
  delete g_user_manager;
  g_user_manager = NULL;
}

// static
UserManager* UserManager::Get() {
  CHECK(g_user_manager);
  return g_user_manager;
}

UserManager::~UserManager() {
}

// static
UserManager* UserManager::SetForTesting(UserManager* user_manager) {
  UserManager* previous_user_manager = g_user_manager;
  if (previous_user_manager)
    previous_user_manager->Shutdown();

  g_user_manager = user_manager;
  return previous_user_manager;
}

ScopedUserManagerEnabler::ScopedUserManagerEnabler(UserManager* user_manager)
    : previous_user_manager_(UserManager::SetForTesting(user_manager)) {
}

ScopedUserManagerEnabler::~ScopedUserManagerEnabler() {
  UserManager::Get()->Shutdown();
  UserManager::Destroy();
  UserManager::SetForTesting(previous_user_manager_);
}

ScopedTestUserManager::ScopedTestUserManager() {
  UserManager::Initialize();

  // ProfileHelper has to be initialized after UserManager instance is created.
  ProfileHelper::Get()->Initialize();
}

ScopedTestUserManager::~ScopedTestUserManager() {
  UserManager::Get()->Shutdown();
  UserManager::Destroy();
}

}  // namespace chromeos
