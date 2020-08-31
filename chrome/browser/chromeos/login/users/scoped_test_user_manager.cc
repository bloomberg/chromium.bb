// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"

#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"

namespace chromeos {

ScopedTestUserManager::ScopedTestUserManager() {
  chrome_user_manager_ = ChromeUserManagerImpl::CreateChromeUserManager();
  chrome_user_manager_->Initialize();

  // ProfileHelper has to be initialized after UserManager instance is created.
  ProfileHelper::Get()->Initialize();
}

ScopedTestUserManager::~ScopedTestUserManager() {
  user_manager::UserManager::Get()->Shutdown();
  chrome_user_manager_->Destroy();
  chrome_user_manager_.reset();
}

}  // namespace chromeos
