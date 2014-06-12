// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu_actions_desktop.h"

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/site_instance.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"

AvatarMenuActionsDesktop::AvatarMenuActionsDesktop() {
}

AvatarMenuActionsDesktop::~AvatarMenuActionsDesktop() {
}

// static
AvatarMenuActions* AvatarMenuActions::Create() {
  return new AvatarMenuActionsDesktop();
}

void AvatarMenuActionsDesktop::AddNewProfile(ProfileMetrics::ProfileAdd type) {
  // TODO: Remove dependency on Browser by delegating AddNewProfile and
  // and EditProfile actions.

  Browser* settings_browser = browser_;
  if (!settings_browser) {
    const Browser::CreateParams params(ProfileManager::GetLastUsedProfile(),
                                       chrome::GetActiveDesktop());
    settings_browser = new Browser(params);
  }
  chrome::ShowSettingsSubPage(settings_browser, chrome::kCreateProfileSubPage);
  ProfileMetrics::LogProfileAddNewUser(type);
}

void AvatarMenuActionsDesktop::EditProfile(Profile* profile, size_t index) {
  Browser* settings_browser = browser_;
  if (!settings_browser) {
    settings_browser = new Browser(
        Browser::CreateParams(profile, chrome::GetActiveDesktop()));
  }
  std::string page = chrome::kManageProfileSubPage;
  page += "#";
  page += base::IntToString(static_cast<int>(index));
  chrome::ShowSettingsSubPage(settings_browser, page);
}

bool AvatarMenuActionsDesktop::ShouldShowAddNewProfileLink() const {
  // |browser_| can be NULL in unit_tests.
  if (browser_ && browser_->profile()->IsSupervised())
    return false;
#if defined(OS_WIN)
  return chrome::GetActiveDesktop() != chrome::HOST_DESKTOP_TYPE_ASH;
#else
  return true;
#endif
}

bool AvatarMenuActionsDesktop::ShouldShowEditProfileLink() const {
#if defined(OS_WIN)
  return chrome::GetActiveDesktop() != chrome::HOST_DESKTOP_TYPE_ASH;
#else
  return true;
#endif
}

void AvatarMenuActionsDesktop::ActiveBrowserChanged(Browser* browser) {
  browser_ = browser;
}
