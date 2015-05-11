// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_base_button.h"

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "components/signin/core/common/profile_management_switches.h"

AvatarBaseButton::AvatarBaseButton(Browser* browser)
    : browser_(browser) {
  // |profile_manager| might be null in tests.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager)
    profile_manager->GetProfileInfoCache().AddObserver(this);
}

AvatarBaseButton::~AvatarBaseButton() {
  // |profile_manager| might be null in tests.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager)
    profile_manager->GetProfileInfoCache().RemoveObserver(this);
}

void AvatarBaseButton::OnProfileAdded(const base::FilePath& profile_path) {
  Update();
}

void AvatarBaseButton::OnProfileWasRemoved(
      const base::FilePath& profile_path,
      const base::string16& profile_name) {
  // If deleting the active profile, don't bother updating the avatar
  // button, as the browser window is being closed anyway.
  if (browser_->profile()->GetPath() != profile_path)
    Update();
}

void AvatarBaseButton::OnProfileNameChanged(
      const base::FilePath& profile_path,
      const base::string16& old_profile_name) {
  if (browser_->profile()->GetPath() == profile_path)
    Update();
}

void AvatarBaseButton::OnProfileAvatarChanged(
      const base::FilePath& profile_path) {
  if (!switches::IsNewAvatarMenu() &&
      browser_->profile()->GetPath() == profile_path)
    Update();
}

void AvatarBaseButton::OnProfileSupervisedUserIdChanged(
      const base::FilePath& profile_path) {
  if (browser_->profile()->GetPath() == profile_path)
    Update();
}
