// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_tab_provider.h"

#include "build/build_config.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

StartupTabs StartupTabProviderImpl::GetOnboardingTabs() const {
#if defined(OS_WIN)
  // Windows 10 has unique onboarding policies and content.
  if (base::win::GetVersion() >= base::win::VERSION_WIN10) {
    // TODO(tmartino): * Add a function, GetWin10SystemState, which gathers
    //                   system state relevant to Win10 Onboarding and returns
    //                   a struct.
    //                 * Add a function, CheckWin10OnboardingTabPolicy, which
    //                   takes such a struct as input and returns StartupTabs.
    return StartupTabs();
  }
#endif

  return CheckStandardOnboardingTabPolicy(first_run::IsChromeFirstRun());
}

StartupTabs StartupTabProviderImpl::GetDistributionFirstRunTabs(
    StartupBrowserCreator* browser_creator) const {
  if (!browser_creator)
    return StartupTabs();
  StartupTabs tabs = CheckMasterPrefsTabPolicy(
      first_run::IsChromeFirstRun(), browser_creator->first_run_tabs_);
  browser_creator->first_run_tabs_.clear();
  return tabs;
}

StartupTabs StartupTabProviderImpl::GetResetTriggerTabs(
    Profile* profile) const {
  auto* triggered_profile_resetter =
      TriggeredProfileResetterFactory::GetForBrowserContext(profile);
  bool has_reset_trigger = triggered_profile_resetter &&
                           triggered_profile_resetter->HasResetTrigger();
  return CheckResetTriggerTabPolicy(has_reset_trigger);
}

StartupTabs StartupTabProviderImpl::GetPinnedTabs(Profile* profile) const {
  return PinnedTabCodec::ReadPinnedTabs(profile);
}

StartupTabs StartupTabProviderImpl::GetPreferencesTabs(
    const base::CommandLine& command_line,
    Profile* profile) const {
  return CheckPreferencesTabPolicy(
      StartupBrowserCreator::GetSessionStartupPref(command_line, profile));
}

// static
StartupTabs StartupTabProviderImpl::CheckStandardOnboardingTabPolicy(
    bool is_first_run) {
  StartupTabs tabs;
  if (is_first_run)
    tabs.emplace_back(GetWelcomePageUrl(), false);
  return tabs;
}

// static
StartupTabs StartupTabProviderImpl::CheckMasterPrefsTabPolicy(
    bool is_first_run,
    const std::vector<GURL>& first_run_tabs) {
  // Constants: Magic words used by Master Preferences files in place of a URL
  // host to indicate that internal pages should appear on first run.
  static constexpr char kNewTabUrlHost[] = "new_tab_page";
  static constexpr char kWelcomePageUrlHost[] = "welcome_page";

  StartupTabs tabs;
  if (is_first_run) {
    tabs.reserve(first_run_tabs.size());
    for (GURL url : first_run_tabs) {
      if (url.host() == kNewTabUrlHost)
        url = GURL(chrome::kChromeUINewTabURL);
      else if (url.host() == kWelcomePageUrlHost)
        url = GetWelcomePageUrl();
      tabs.emplace_back(url, false);
    }
  }
  return tabs;
}

// static
StartupTabs StartupTabProviderImpl::CheckResetTriggerTabPolicy(
    bool profile_has_trigger) {
  StartupTabs tabs;
  if (profile_has_trigger)
    tabs.emplace_back(GetTriggeredResetSettingsUrl(), false);
  return tabs;
}

// static
StartupTabs StartupTabProviderImpl::CheckPreferencesTabPolicy(
    SessionStartupPref pref) {
  StartupTabs tabs;
  if (pref.type == SessionStartupPref::Type::URLS && !pref.urls.empty()) {
    for (auto& url : pref.urls)
      tabs.push_back(StartupTab(url, false));
  }
  return tabs;
}

// static
GURL StartupTabProviderImpl::GetWelcomePageUrl() {
  return GURL(chrome::kChromeUIWelcomeURL);
}

// static
GURL StartupTabProviderImpl::GetTriggeredResetSettingsUrl() {
  return GURL(
      chrome::GetSettingsUrl(chrome::kTriggeredResetProfileSettingsSubPage));
}
