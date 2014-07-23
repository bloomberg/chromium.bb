// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/profiles/profile_list_chromeos.h"

#include <algorithm>

#include "ash/shell.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "components/signin/core/common/profile_management_switches.h"

// static
ProfileList* ProfileList::Create(ProfileInfoInterface* profile_cache) {
  return new chromeos::ProfileListChromeOS(profile_cache);
}

namespace chromeos {

ProfileListChromeOS::ProfileListChromeOS(ProfileInfoInterface* profile_cache)
    : profile_info_(profile_cache) {
}

ProfileListChromeOS::~ProfileListChromeOS() {
  ClearMenu();
}

size_t ProfileListChromeOS::GetNumberOfItems() const {
  return items_.size();
}

const AvatarMenu::Item& ProfileListChromeOS::GetItemAt(size_t index) const {
  DCHECK_LT(index, items_.size());
  return *items_[index];
}

void ProfileListChromeOS::RebuildMenu() {
  ClearMenu();

  // Filter for profiles associated with logged-in users.
  user_manager::UserList users = UserManager::Get()->GetLoggedInUsers();

  // Add corresponding profiles.
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end();
       ++it) {
    size_t i = profile_info_->GetIndexOfProfileWithPath(
        ProfileHelper::GetProfilePathByUserIdHash((*it)->username_hash()));

    gfx::Image icon = gfx::Image((*it)->GetImage());
    if (!switches::IsNewProfileManagement() && !icon.IsEmpty()) {
      // old avatar menu uses resized-small images
      icon = profiles::GetAvatarIconForMenu(icon, true);
    }

    AvatarMenu::Item* item = new AvatarMenu::Item(i, i, icon);
    item->name = (*it)->GetDisplayName();
    item->sync_state = profile_info_->GetUserNameOfProfileAtIndex(i);
    item->profile_path = profile_info_->GetPathOfProfileAtIndex(i);
    item->supervised = false;
    item->signed_in = true;
    item->active = profile_info_->GetPathOfProfileAtIndex(i) ==
        active_profile_path_;
    item->signin_required = profile_info_->ProfileIsSigninRequiredAtIndex(i);
    items_.push_back(item);
  }

  SortMenu();

  // After sorting, assign items their actual indices.
  for (size_t i = 0; i < items_.size(); ++i)
    items_[i]->menu_index = i;
}

size_t ProfileListChromeOS::MenuIndexFromProfileIndex(size_t index) {
  // On ChromeOS, the active profile might be Default, which does not show
  // up in the model as a logged-in user. In that case, we return 0.
  size_t menu_index = 0;

  for (size_t i = 0; i < GetNumberOfItems(); ++i) {
    if (items_[i]->profile_index == index) {
      menu_index = i;
      break;
    }
  }

  return menu_index;
}

void ProfileListChromeOS::ActiveProfilePathChanged(base::FilePath& path) {
  active_profile_path_ = path;
}

void ProfileListChromeOS::ClearMenu() {
  STLDeleteElements(&items_);
}

void ProfileListChromeOS::SortMenu() {
  // Sort list of items by name.
  std::sort(items_.begin(), items_.end(), &AvatarMenu::CompareItems);
}

}  // namespace chromeos
