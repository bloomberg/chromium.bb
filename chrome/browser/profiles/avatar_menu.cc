// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu.h"

#include "ash/ash_switches.h"
#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/avatar_menu_actions.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_list.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/ash/chrome_shell_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

using content::BrowserThread;

namespace {

// Constants for the show profile switcher experiment
const char kShowProfileSwitcherFieldTrialName[] = "ShowProfileSwitcher";
const char kAlwaysShowSwitcherGroupName[] = "AlwaysShow";

}  // namespace

AvatarMenu::AvatarMenu(ProfileInfoInterface* profile_cache,
                       AvatarMenuObserver* observer,
                       Browser* browser)
    : profile_list_(ProfileList::Create(profile_cache)),
      menu_actions_(AvatarMenuActions::Create()),
      profile_info_(profile_cache),
      observer_(observer),
      browser_(browser) {
  DCHECK(profile_info_);
  // Don't DCHECK(browser_) so that unit tests can reuse this ctor.

  ActiveBrowserChanged(browser_);

  // Register this as an observer of the info cache.
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      content::NotificationService::AllSources());
}

AvatarMenu::~AvatarMenu() {
}

AvatarMenu::Item::Item(size_t menu_index,
                       size_t profile_index,
                       const gfx::Image& icon)
    : icon(icon),
      active(false),
      signed_in(false),
      signin_required(false),
      menu_index(menu_index),
      profile_index(profile_index) {
}

AvatarMenu::Item::~Item() {
}

// static
bool AvatarMenu::ShouldShowAvatarMenu() {
  if (base::FieldTrialList::FindFullName(kShowProfileSwitcherFieldTrialName) ==
      kAlwaysShowSwitcherGroupName) {
    // We should only be in this group when multi-profiles is enabled.
    DCHECK(profiles::IsMultipleProfilesEnabled());
    return true;
  }

  // TODO: Eliminate this ifdef. Add a delegate interface for the menu which
  // would also help remove the Browser dependency in AvatarMenuActions
  // implementations.
  if (profiles::IsMultipleProfilesEnabled()) {
#if defined(OS_CHROMEOS)
    // On ChromeOS the menu will not be shown.
    return false;
#else
    return switches::IsNewAvatarMenu() ||
           (g_browser_process->profile_manager() &&
            g_browser_process->profile_manager()->GetNumberOfProfiles() > 1);
#endif
  }
  return false;
}

bool AvatarMenu::CompareItems(const Item* item1, const Item* item2) {
  return base::i18n::ToLower(item1->name).compare(
      base::i18n::ToLower(item2->name)) < 0;
}

void AvatarMenu::SwitchToProfile(size_t index,
                                 bool always_create,
                                 ProfileMetrics::ProfileOpen metric) {
  DCHECK(profiles::IsMultipleProfilesEnabled() ||
         index == GetActiveProfileIndex());
  const Item& item = GetItemAt(index);

  if (switches::IsNewAvatarMenu()) {
    // Don't open a browser window for signed-out profiles.
    if (item.signin_required) {
      chrome::ShowUserManager(item.profile_path);
      return;
    }
  }

  base::FilePath path =
      profile_info_->GetPathOfProfileAtIndex(item.profile_index);

  chrome::HostDesktopType desktop_type = chrome::GetActiveDesktop();
  if (browser_)
    desktop_type = browser_->host_desktop_type();

  profiles::SwitchToProfile(path, desktop_type, always_create,
                            profiles::ProfileSwitchingDoneCallback(),
                            metric);
}

void AvatarMenu::AddNewProfile(ProfileMetrics::ProfileAdd type) {
  menu_actions_->AddNewProfile(type);
}

void AvatarMenu::EditProfile(size_t index) {
  // Get the index in the profile cache from the menu index.
  size_t profile_index = profile_list_->GetItemAt(index).profile_index;

  Profile* profile = g_browser_process->profile_manager()->GetProfileByPath(
        profile_info_->GetPathOfProfileAtIndex(profile_index));

  menu_actions_->EditProfile(profile, profile_index);
}

void AvatarMenu::RebuildMenu() {
  profile_list_->RebuildMenu();
}

size_t AvatarMenu::GetNumberOfItems() const {
  return profile_list_->GetNumberOfItems();
}

const AvatarMenu::Item& AvatarMenu::GetItemAt(size_t index) const {
  return profile_list_->GetItemAt(index);
}
size_t AvatarMenu::GetActiveProfileIndex() {

  // During singleton profile deletion, this function can be called with no
  // profiles in the model - crbug.com/102278 .
  if (profile_list_->GetNumberOfItems() == 0)
    return 0;

  Profile* active_profile = NULL;
  if (!browser_)
    active_profile = ProfileManager::GetLastUsedProfile();
  else
    active_profile = browser_->profile();

  size_t index =
      profile_info_->GetIndexOfProfileWithPath(active_profile->GetPath());

  index = profile_list_->MenuIndexFromProfileIndex(index);
  DCHECK_LT(index, profile_list_->GetNumberOfItems());
  return index;
}

base::string16 AvatarMenu::GetSupervisedUserInformation() const {
  // |browser_| can be NULL in unit_tests.
  if (browser_ && browser_->profile()->IsSupervised()) {
#if defined(ENABLE_MANAGED_USERS)
    SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(browser_->profile());
    base::string16 custodian =
        base::UTF8ToUTF16(service->GetCustodianEmailAddress());
    return l10n_util::GetStringFUTF16(IDS_SUPERVISED_USER_INFO, custodian);
#endif
  }
  return base::string16();
}

const gfx::Image& AvatarMenu::GetSupervisedUserIcon() const {
  return ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_SUPERVISED_USER_ICON);
}

void AvatarMenu::ActiveBrowserChanged(Browser* browser) {
  browser_ = browser;
  menu_actions_->ActiveBrowserChanged(browser);

  // If browser is not NULL, get the path of its active profile.
  base::FilePath path;
  if (browser)
    path = browser->profile()->GetPath();
  profile_list_->ActiveProfilePathChanged(path);
}

bool AvatarMenu::ShouldShowAddNewProfileLink() const {
  return menu_actions_->ShouldShowAddNewProfileLink();
}

bool AvatarMenu::ShouldShowEditProfileLink() const {
  return menu_actions_->ShouldShowEditProfileLink();
}

void AvatarMenu::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED, type);
  RebuildMenu();
  if (observer_)
    observer_->OnAvatarMenuChanged(this);
}
