// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"

#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"

namespace chromeos {

ScopedUserManagerEnabler::ScopedUserManagerEnabler(
    ChromeUserManager* user_manager) {
  if (user_manager::UserManager::GetForTesting())
    user_manager::UserManager::GetForTesting()->Shutdown();

  previous_user_manager_ =
      user_manager::UserManager::SetForTesting(user_manager);
}

ScopedUserManagerEnabler::~ScopedUserManagerEnabler() {
  // Shutdown and destroy current UserManager instance that we track.
  user_manager::UserManager::Get()->Shutdown();
  delete user_manager::UserManager::Get();
  user_manager::UserManager::SetInstance(nullptr);

  user_manager::UserManager::SetForTesting(previous_user_manager_);
}

}  // namespace chromeos
