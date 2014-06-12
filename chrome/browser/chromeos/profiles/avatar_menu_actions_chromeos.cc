// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/profiles/avatar_menu_actions_chromeos.h"

#include "ash/multi_profile_uma.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/browser.h"

// static
AvatarMenuActions* AvatarMenuActions::Create() {
  return new chromeos::AvatarMenuActionsChromeOS();
}

namespace chromeos {

AvatarMenuActionsChromeOS::AvatarMenuActionsChromeOS() {
}

AvatarMenuActionsChromeOS::~AvatarMenuActionsChromeOS() {
}

void AvatarMenuActionsChromeOS::AddNewProfile(ProfileMetrics::ProfileAdd type) {
  // Let the user add another account to the session.
  ash::MultiProfileUMA::RecordSigninUser(
      ash::MultiProfileUMA::SIGNIN_USER_BY_BROWSER_FRAME);
  ash::Shell::GetInstance()->system_tray_delegate()->ShowUserLogin();
}

void AvatarMenuActionsChromeOS::EditProfile(Profile* profile, size_t index) {
  NOTIMPLEMENTED();
}

bool AvatarMenuActionsChromeOS::ShouldShowAddNewProfileLink() const {
  // |browser_| can be NULL in unit_tests.
  return (!browser_ || !browser_->profile()->IsSupervised()) &&
      UserManager::Get()->GetUsersAdmittedForMultiProfile().size();
}

bool AvatarMenuActionsChromeOS::ShouldShowEditProfileLink() const {
  return false;
}

void AvatarMenuActionsChromeOS::ActiveBrowserChanged(Browser* browser) {
  browser_ = browser;
}

}  // namespace chromeos
