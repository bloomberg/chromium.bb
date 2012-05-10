// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu_model.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu_model_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_init.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

void OnProfileCreated(bool always_create,
                      Profile* profile,
                      Profile::CreateStatus status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    ProfileManager::FindOrCreateNewWindowForProfile(
        profile,
        BrowserInit::IS_NOT_PROCESS_STARTUP,
        BrowserInit::IS_NOT_FIRST_RUN,
        always_create);
  }
}

}  // namespace

AvatarMenuModel::AvatarMenuModel(ProfileInfoInterface* profile_cache,
                                 AvatarMenuModelObserver* observer,
                                 Browser* browser)
    : profile_info_(profile_cache),
      observer_(observer),
      browser_(browser) {
  DCHECK(profile_info_);
  // Don't DCHECK(browser_) so that unit tests can reuse this ctor.

  // Register this as an observer of the info cache.
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      content::NotificationService::AllSources());

  // Build the initial menu.
  RebuildMenu();
}

AvatarMenuModel::~AvatarMenuModel() {
  ClearMenu();
}

AvatarMenuModel::Item::Item(size_t model_index, const gfx::Image& icon)
    : icon(icon),
      active(false),
      model_index(model_index) {
}

AvatarMenuModel::Item::~Item() {
}

void AvatarMenuModel::SwitchToProfile(size_t index, bool always_create) {
  DCHECK(ProfileManager::IsMultipleProfilesEnabled() ||
         index == GetActiveProfileIndex());
  const Item& item = GetItemAt(index);
  FilePath path = profile_info_->GetPathOfProfileAtIndex(item.model_index);
  g_browser_process->profile_manager()->CreateProfileAsync(
      path, base::Bind(&OnProfileCreated, always_create));

  ProfileMetrics::LogProfileSwitchUser(ProfileMetrics::SWITCH_PROFILE_ICON);
}

void AvatarMenuModel::EditProfile(size_t index) {
  Browser* browser = browser_;
  if (!browser) {
    Profile* profile = g_browser_process->profile_manager()->GetProfileByPath(
        profile_info_->GetPathOfProfileAtIndex(GetItemAt(index).model_index));
    browser = Browser::Create(profile);
  }
  std::string page = chrome::kManageProfileSubPage;
  page += "#";
  page += base::IntToString(static_cast<int>(index));
  browser->ShowOptionsTab(page);
}

void AvatarMenuModel::AddNewProfile() {
  ProfileManager::CreateMultiProfileAsync();
  ProfileMetrics::LogProfileAddNewUser(ProfileMetrics::ADD_NEW_USER_ICON);
}

size_t AvatarMenuModel::GetNumberOfItems() {
  return items_.size();
}

size_t AvatarMenuModel::GetActiveProfileIndex() {
  // During singleton profile deletion, this function can be called with no
  // profiles in the model - crbug.com/102278 .
  if (items_.size() == 0)
    return 0;

  Profile* active_profile = NULL;
  if (!browser_)
    active_profile = ProfileManager::GetLastUsedProfile();
  else
    active_profile = browser_->profile();

  size_t index =
      profile_info_->GetIndexOfProfileWithPath(active_profile->GetPath());

  DCHECK_LT(index, items_.size());
  return index;
}

const AvatarMenuModel::Item& AvatarMenuModel::GetItemAt(size_t index) {
  DCHECK_LT(index, items_.size());
  return *items_[index];
}

void AvatarMenuModel::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED, type);
  RebuildMenu();
  if (observer_)
    observer_->OnAvatarMenuModelChanged(this);
}

// static
bool AvatarMenuModel::ShouldShowAvatarMenu() {
  return ProfileManager::IsMultipleProfilesEnabled() &&
      g_browser_process->profile_manager()->GetNumberOfProfiles() > 1;
}

void AvatarMenuModel::RebuildMenu() {
  ClearMenu();

  const size_t count = profile_info_->GetNumberOfProfiles();
  for (size_t i = 0; i < count; ++i) {
    bool is_gaia_picture =
        profile_info_->IsUsingGAIAPictureOfProfileAtIndex(i) &&
        profile_info_->GetGAIAPictureOfProfileAtIndex(i);
    gfx::Image icon = profiles::GetAvatarIconForMenu(
        profile_info_->GetAvatarIconOfProfileAtIndex(i), is_gaia_picture);

    Item* item = new Item(i, icon);
    item->name = profile_info_->GetNameOfProfileAtIndex(i);
    item->sync_state = profile_info_->GetUserNameOfProfileAtIndex(i);
    item->signed_in = !item->sync_state.empty();
    if (!item->signed_in) {
      item->sync_state = l10n_util::GetStringUTF16(
          IDS_PROFILES_LOCAL_PROFILE_STATE);
    }
    if (browser_) {
      FilePath path = profile_info_->GetPathOfProfileAtIndex(i);
      item->active = browser_->profile()->GetPath() == path;
    }
    items_.push_back(item);
  }
}

void AvatarMenuModel::ClearMenu() {
  STLDeleteContainerPointers(items_.begin(), items_.end());
  items_.clear();
}
