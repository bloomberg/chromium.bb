// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/profiles/avatar_menu_actions_chromeos.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile_info_util.h"
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
  ash::Shell::GetInstance()->system_tray_delegate()->ShowUserLogin();
}

void AvatarMenuActionsChromeOS::EditProfile(Profile* profile, size_t index) {
  NOTIMPLEMENTED();
}

bool AvatarMenuActionsChromeOS::ShouldShowAddNewProfileLink() const {
  // |browser_| can be NULL in unit_tests.
  return (!browser_ || !browser_->profile()->IsManaged()) &&
      UserManager::Get()->GetUsersAdmittedForMultiProfile().size();
}

bool AvatarMenuActionsChromeOS::ShouldShowEditProfileLink() const {
  return false;
}

content::WebContents* AvatarMenuActionsChromeOS::BeginSignOut() {
  NOTIMPLEMENTED();
  return NULL;
}

void AvatarMenuActionsChromeOS::SetLogoutURL(const std::string& logout_url) {
  NOTIMPLEMENTED();
}

void AvatarMenuActionsChromeOS::ActiveBrowserChanged(Browser* browser) {
  browser_ = browser;
}

}  // namespace chromeos
