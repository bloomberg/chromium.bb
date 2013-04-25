// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_manager.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/user_manager_impl.h"

namespace chromeos {

// static
const char UserManager::kStubUser[] = "stub-user@example.com";

// static
const char UserManager::kLocallyManagedUserDomain[] =
    "locally-managed.localhost";

// static
const char UserManager::kKioskAppUserDomain[] = "kiosk-apps.localhost";

static UserManager* g_user_manager = NULL;

UserManager::Observer::~Observer() {
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
