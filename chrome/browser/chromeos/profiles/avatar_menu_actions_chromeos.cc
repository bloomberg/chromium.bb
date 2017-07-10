// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/profiles/avatar_menu_actions_chromeos.h"

#include "ash/multi_profile_uma.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/browser/ui/browser.h"
#include "components/user_manager/user_manager.h"

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
  SessionControllerClient::Get()->ShowMultiProfileLogin();
}

void AvatarMenuActionsChromeOS::EditProfile(Profile* profile) {
  NOTIMPLEMENTED();
}

bool AvatarMenuActionsChromeOS::ShouldShowAddNewProfileLink() const {
  // |browser_| can be NULL in unit_tests.
  return (!browser_ || !browser_->profile()->IsSupervised()) &&
         user_manager::UserManager::Get()
             ->GetUsersAllowedForMultiProfile()
             .size();
}

bool AvatarMenuActionsChromeOS::ShouldShowEditProfileLink() const {
  return false;
}

void AvatarMenuActionsChromeOS::ActiveBrowserChanged(Browser* browser) {
  browser_ = browser;
}

}  // namespace chromeos
