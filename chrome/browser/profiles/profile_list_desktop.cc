// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_list_desktop.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

ProfileListDesktop::ProfileListDesktop(ProfileInfoInterface* profile_cache)
    : profile_info_(profile_cache) {
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
    bool is_gaia_picture =
        profile_info_->IsUsingGAIAPictureOfProfileAtIndex(i) &&
        profile_info_->GetGAIAPictureOfProfileAtIndex(i);

    gfx::Image icon = profile_info_->GetAvatarIconOfProfileAtIndex(i);
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kNewProfileManagement)) {
      // old avatar menu uses resized-small images
      icon = profiles::GetAvatarIconForMenu(icon, is_gaia_picture);
    }

    AvatarMenu::Item* item = new AvatarMenu::Item(i, i, icon);
    item->name = profile_info_->GetNameOfProfileAtIndex(i);
    item->sync_state = profile_info_->GetUserNameOfProfileAtIndex(i);
    item->profile_path = profile_info_->GetPathOfProfileAtIndex(i);
    item->managed = profile_info_->ProfileIsManagedAtIndex(i);
    item->signed_in = !item->sync_state.empty();
    if (!item->signed_in) {
      item->sync_state = l10n_util::GetStringUTF16(
          item->managed ? IDS_MANAGED_USER_AVATAR_LABEL :
                          IDS_PROFILES_LOCAL_PROFILE_STATE);
    }
    item->active = profile_info_->GetPathOfProfileAtIndex(i) ==
        active_profile_path_;
    item->signin_required = profile_info_->ProfileIsSigninRequiredAtIndex(i);
    items_.push_back(item);
  }
}

size_t ProfileListDesktop::MenuIndexFromProfileIndex(size_t index) {
  // Menu indices correspond to indices in profile cache.
  return index;
}

void ProfileListDesktop::ActiveProfilePathChanged(base::FilePath& path) {
  active_profile_path_ = path;
}

void ProfileListDesktop::ClearMenu() {
  STLDeleteElements(&items_);
}
