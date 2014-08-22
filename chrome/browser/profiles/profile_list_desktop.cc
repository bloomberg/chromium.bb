// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_list_desktop.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "ui/base/l10n/l10n_util.h"

ProfileListDesktop::ProfileListDesktop(ProfileInfoInterface* profile_cache)
    : profile_info_(profile_cache),
      omitted_item_count_(0) {
}

ProfileListDesktop::~ProfileListDesktop() {
  ClearMenu();
}

// static
ProfileList* ProfileList::Create(ProfileInfoInterface* profile_cache) {
  return new ProfileListDesktop(profile_cache);
}

size_t ProfileListDesktop::GetNumberOfItems() const {
  return items_.size();
}

const AvatarMenu::Item& ProfileListDesktop::GetItemAt(size_t index) const {
  DCHECK_LT(index, items_.size());
  return *items_[index];
}

void ProfileListDesktop::RebuildMenu() {
  ClearMenu();

  const size_t count = profile_info_->GetNumberOfProfiles();
  for (size_t i = 0; i < count; ++i) {
    if (profile_info_->IsOmittedProfileAtIndex(i)) {
      omitted_item_count_++;
      continue;
    }

    gfx::Image icon = profile_info_->GetAvatarIconOfProfileAtIndex(i);
    AvatarMenu::Item* item = new AvatarMenu::Item(i - omitted_item_count_,
                                                  i,
                                                  icon);
    item->name = profile_info_->GetNameOfProfileAtIndex(i);
    item->sync_state = profile_info_->GetUserNameOfProfileAtIndex(i);
    item->profile_path = profile_info_->GetPathOfProfileAtIndex(i);
    item->supervised = profile_info_->ProfileIsSupervisedAtIndex(i);
    item->signed_in = !item->sync_state.empty();
    if (!item->signed_in) {
      item->sync_state = l10n_util::GetStringUTF16(
          item->supervised ? IDS_SUPERVISED_USER_AVATAR_LABEL :
                             IDS_PROFILES_LOCAL_PROFILE_STATE);
    }
    item->active = profile_info_->GetPathOfProfileAtIndex(i) ==
        active_profile_path_;
    item->signin_required = profile_info_->ProfileIsSigninRequiredAtIndex(i);
    items_.push_back(item);
  }
  // One omitted item is expected when a supervised-user profile is in the
  // process of being registered, but there shouldn't be more than one.
  VLOG_IF(2, (omitted_item_count_ > 1)) << omitted_item_count_
                                        << " profiles omitted fom list.";
}

size_t ProfileListDesktop::MenuIndexFromProfileIndex(size_t index) {
  const size_t menu_count = GetNumberOfItems();
  DCHECK_LT(index, menu_count + omitted_item_count_);

  // In the common case, valid profile-cache indices correspond to indices in
  // the menu.
  if (!omitted_item_count_)
    return index;

  for (size_t i = 0; i < menu_count; ++i) {
    const AvatarMenu::Item item = GetItemAt(i);
    if (item.profile_index == index)
      return i;
  }

  // The desired index was not found; return a fallback value.
  NOTREACHED();
  return 0;
}

void ProfileListDesktop::ActiveProfilePathChanged(base::FilePath& path) {
  active_profile_path_ = path;
}

void ProfileListDesktop::ClearMenu() {
  STLDeleteElements(&items_);
  omitted_item_count_ = 0;
}
