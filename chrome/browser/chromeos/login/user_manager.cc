// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_manager.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/chromeos/login/user_manager_impl.h"
#include "chrome/common/chrome_switches.h"

namespace chromeos {

// static
const char UserManager::kStubUser[] = "stub-user@example.com";

// static
// Should match cros constant in platform/libchromeos/chromeos/cryptohome.h
const char UserManager::kGuestUserName[] = "$guest";

// static
const char UserManager::kLocallyManagedUserDomain[] =
    "locally-managed.localhost";


// static
const char UserManager::kRetailModeUserName[] = "demouser@";
static UserManager* g_user_manager = NULL;

UserManager::Observer::~Observer() {
}

void UserManager::Observer::LocalStateChanged(UserManager* user_manager) {
}

void UserManager::UserSessionStateObserver::ActiveUserChanged(
    const User* active_user) {
}

void UserManager::UserSessionStateObserver::ActiveUserHashChanged(
    const std::string& hash) {
}

void UserManager::UserSessionStateObserver::
PendingUserSessionsRestoreFinished() {
}

UserManager::UserSessionStateObserver::~UserSessionStateObserver() {
}

// static
void UserManager::Initialize() {
  CHECK(!g_user_manager);
  g_user_manager = new UserManagerImpl();
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

// static
bool UserManager::IsMultipleProfilesAllowed() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(::switches::kMultiProfiles))
    return false;

  // TODO(xiyuan): Get rid of this when the underlying support is ready.
  const char kFieldTrialName[] = "ChromeOSUseMultiProfiles";
  const char kEnable[] = "Enable";
  return base::FieldTrialList::FindFullName(kFieldTrialName) == kEnable;
}

UserManager::~UserManager() {
}

// static
UserManager* UserManager::SetForTesting(UserManager* user_manager) {
  UserManager* previous_user_manager = g_user_manager;
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
}

ScopedTestUserManager::~ScopedTestUserManager() {
  UserManager::Get()->Shutdown();
  UserManager::Destroy();
}

}  // namespace chromeos
