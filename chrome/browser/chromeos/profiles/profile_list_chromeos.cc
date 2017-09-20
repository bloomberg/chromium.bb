// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/profiles/profile_list_chromeos.h"

#include <unordered_map>
#include <utility>

#include "base/command_line.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "components/user_manager/user_manager.h"

// static
ProfileList* ProfileList::Create(ProfileAttributesStorage* profile_storage) {
  return new chromeos::ProfileListChromeOS(profile_storage);
}

namespace chromeos {

ProfileListChromeOS::ProfileListChromeOS(
    ProfileAttributesStorage* profile_storage)
    : profile_storage_(profile_storage) {}

ProfileListChromeOS::~ProfileListChromeOS() {
}

size_t ProfileListChromeOS::GetNumberOfItems() const {
  return items_.size();
}

const AvatarMenu::Item& ProfileListChromeOS::GetItemAt(size_t index) const {
  DCHECK_LT(index, items_.size());
  return *items_[index];
}

void ProfileListChromeOS::RebuildMenu() {
  items_.clear();

  // Only the profiles associated with logged-in users are added to the menu.
  user_manager::UserList users =
      user_manager::UserManager::Get()->GetLoggedInUsers();

  // Create a mapping from profile path to the corresponding entries in |users|.
  std::unordered_map<base::FilePath::StringType, size_t> user_indices_by_path;
  for (size_t i = 0; i < users.size(); ++i) {
    user_indices_by_path.insert(std::make_pair(
        ProfileHelper::GetProfilePathByUserIdHash(users[i]->username_hash())
            .value(),
        i));
  }
  DCHECK_EQ(user_indices_by_path.size(), users.size());

  std::vector<ProfileAttributesEntry*> entries =
      profile_storage_->GetAllProfilesAttributesSortedByName();

  // Add the profiles.
  for (ProfileAttributesEntry* entry : entries) {
    auto user_index_it = user_indices_by_path.find(entry->GetPath().value());
    if (user_index_it == user_indices_by_path.end())
      continue;
    user_manager::User* user = users[user_index_it->second];

    gfx::Image icon = gfx::Image(user->GetImage());
    std::unique_ptr<AvatarMenu::Item> item(
        new AvatarMenu::Item(items_.size(), entry->GetPath(), icon));
    item->name = user->GetDisplayName();
    item->username = entry->GetUserName();
    DCHECK(!entry->IsLegacySupervised());
    item->legacy_supervised = false;
    item->child_account = entry->IsChild();
    item->signed_in = true;
    item->active = item->profile_path == active_profile_path_;
    item->signin_required = entry->IsSigninRequired();
    items_.push_back(std::move(item));
  }
}

size_t ProfileListChromeOS::MenuIndexFromProfilePath(
    const base::FilePath& path) const {
  const size_t menu_count = GetNumberOfItems();

  for (size_t i = 0; i < menu_count; ++i) {
    const AvatarMenu::Item item = GetItemAt(i);
    if (item.profile_path == path)
      return i;
  }

  // On ChromeOS, the active profile might be Default, which does not show
  // up in the model as a logged-in user. In that case, we return 0.
  return 0;
}

void ProfileListChromeOS::ActiveProfilePathChanged(
    const base::FilePath& active_profile_path) {
  active_profile_path_ = active_profile_path;
}

}  // namespace chromeos
