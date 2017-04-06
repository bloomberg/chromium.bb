// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_tab_provider.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/locale_settings.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/shell_integration.h"
#endif

namespace {

// Attempts to find an existing, non-empty tabbed browser for this profile.
bool ProfileHasOtherTabbedBrowser(Profile* profile) {
  BrowserList* browser_list = BrowserList::GetInstance();
  auto other_tabbed_browser = std::find_if(
      browser_list->begin(), browser_list->end(), [profile](Browser* browser) {
        return browser->profile() == profile && browser->is_type_tabbed() &&
               !browser->tab_strip_model()->empty();
      });
  return other_tabbed_browser != browser_list->end();
}

}  // namespace

StartupTabs StartupTabProviderImpl::GetOnboardingTabs(Profile* profile) const {
// Onboarding content has not been launched on Chrome OS.
#if defined(OS_CHROMEOS)
  return StartupTabs();
#else
  if (!profile)
    return StartupTabs();

  bool is_first_run = first_run::IsChromeFirstRun();
  PrefService* prefs = profile->GetPrefs();
  bool has_seen_welcome_page =
      prefs && prefs->GetBoolean(prefs::kHasSeenWelcomePage);
  bool is_signin_allowed = profile->IsSyncAllowed();
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  bool is_signed_in = signin_manager && signin_manager->IsAuthenticated();
  bool is_supervised_user = profile->IsSupervised();

#if defined(OS_WIN)
  // Windows 10 has unique onboarding policies and content.
  if (base::win::GetVersion() >= base::win::VERSION_WIN10) {
    PrefService* local_state = g_browser_process->local_state();
    bool has_seen_win10_promo =
        local_state && local_state->GetBoolean(prefs::kHasSeenWin10PromoPage);
    // The set default browser operation can be disabled by the browser
    // distribution (e.g. SxS Canary), or by enterprise policy. In these cases,
    // the Win 10 promo page should not be displayed.
    bool disabled_by_enterprise_policy =
        local_state &&
        local_state->IsManagedPreference(
            prefs::kDefaultBrowserSettingEnabled) &&
        !local_state->GetBoolean(prefs::kDefaultBrowserSettingEnabled);
    bool set_default_browser_allowed =
        !disabled_by_enterprise_policy &&
        shell_integration::CanSetAsDefaultBrowser();
    bool is_default_browser =
        g_browser_process->CachedDefaultWebClientState() ==
        shell_integration::IS_DEFAULT;
    return GetWin10OnboardingTabsForState(
        is_first_run, has_seen_welcome_page, has_seen_win10_promo,
        is_signin_allowed, is_signed_in, set_default_browser_allowed,
        is_default_browser, is_supervised_user);
  }
#endif  // defined(OS_WIN)

  return GetStandardOnboardingTabsForState(is_first_run, has_seen_welcome_page,
                                           is_signin_allowed, is_signed_in,
                                           is_supervised_user);
#endif  // defined(OS_CHROMEOS)
}

StartupTabs StartupTabProviderImpl::GetDistributionFirstRunTabs(
    StartupBrowserCreator* browser_creator) const {
  if (!browser_creator)
    return StartupTabs();
  StartupTabs tabs = GetMasterPrefsTabsForState(
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
  return GetResetTriggerTabsForState(has_reset_trigger);
}

StartupTabs StartupTabProviderImpl::GetPinnedTabs(
    const base::CommandLine& command_line,
    Profile* profile) const {
  return GetPinnedTabsForState(
      StartupBrowserCreator::GetSessionStartupPref(command_line, profile),
      PinnedTabCodec::ReadPinnedTabs(profile),
      ProfileHasOtherTabbedBrowser(profile));
}

StartupTabs StartupTabProviderImpl::GetPreferencesTabs(
    const base::CommandLine& command_line,
    Profile* profile) const {
  return GetPreferencesTabsForState(
      StartupBrowserCreator::GetSessionStartupPref(command_line, profile),
      ProfileHasOtherTabbedBrowser(profile));
}

StartupTabs StartupTabProviderImpl::GetNewTabPageTabs(
    const base::CommandLine& command_line,
    Profile* profile) const {
  return GetNewTabPageTabsForState(
      StartupBrowserCreator::GetSessionStartupPref(command_line, profile));
}

// static
StartupTabs StartupTabProviderImpl::GetStandardOnboardingTabsForState(
    bool is_first_run,
    bool has_seen_welcome_page,
    bool is_signin_allowed,
    bool is_signed_in,
    bool is_supervised_user) {
  StartupTabs tabs;
  if (!has_seen_welcome_page && is_signin_allowed && !is_signed_in &&
      !is_supervised_user)
    tabs.emplace_back(GetWelcomePageUrl(!is_first_run), false);
  return tabs;
}

#if defined(OS_WIN)
// static
StartupTabs StartupTabProviderImpl::GetWin10OnboardingTabsForState(
    bool is_first_run,
    bool has_seen_welcome_page,
    bool has_seen_win10_promo,
    bool is_signin_allowed,
    bool is_signed_in,
    bool set_default_browser_allowed,
    bool is_default_browser,
    bool is_supervised_user) {
  StartupTabs tabs;

  if (is_supervised_user)
    return tabs;

  if (set_default_browser_allowed && !has_seen_win10_promo &&
      !is_default_browser) {
    tabs.emplace_back(GetWin10WelcomePageUrl(!is_first_run), false);
  } else if (!has_seen_welcome_page && is_signin_allowed && !is_signed_in) {
    tabs.emplace_back(GetWelcomePageUrl(!is_first_run), false);
  }
  return tabs;
}
#endif

// static
StartupTabs StartupTabProviderImpl::GetMasterPrefsTabsForState(
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
      if (url.host_piece() == kNewTabUrlHost)
        url = GURL(chrome::kChromeUINewTabURL);
      else if (url.host_piece() == kWelcomePageUrlHost)
        url = GetWelcomePageUrl(false);
      tabs.emplace_back(url, false);
    }
  }
  return tabs;
}

// static
StartupTabs StartupTabProviderImpl::GetResetTriggerTabsForState(
    bool profile_has_trigger) {
  StartupTabs tabs;
  if (profile_has_trigger)
    tabs.emplace_back(GetTriggeredResetSettingsUrl(), false);
  return tabs;
}

// static
StartupTabs StartupTabProviderImpl::GetPinnedTabsForState(
    const SessionStartupPref& pref,
    const StartupTabs& pinned_tabs,
    bool profile_has_other_tabbed_browser) {
  if (pref.type == SessionStartupPref::Type::LAST ||
      profile_has_other_tabbed_browser)
    return StartupTabs();
  return pinned_tabs;
}

// static
StartupTabs StartupTabProviderImpl::GetPreferencesTabsForState(
    const SessionStartupPref& pref,
    bool profile_has_other_tabbed_browser) {
  StartupTabs tabs;
  if (pref.type == SessionStartupPref::Type::URLS && !pref.urls.empty() &&
      !profile_has_other_tabbed_browser) {
    for (const auto& url : pref.urls)
      tabs.push_back(StartupTab(url, false));
  }
  return tabs;
}

// static
StartupTabs StartupTabProviderImpl::GetNewTabPageTabsForState(
    const SessionStartupPref& pref) {
  StartupTabs tabs;
  if (pref.type != SessionStartupPref::Type::LAST)
    tabs.emplace_back(GURL(chrome::kChromeUINewTabURL), false);
  return tabs;
}

// static
GURL StartupTabProviderImpl::GetWelcomePageUrl(bool use_later_run_variant) {
  // Record that the Welcome page was added to the startup url list.
  UMA_HISTOGRAM_BOOLEAN("Welcome.Win10.NewPromoPageAdded", true);
  GURL url(chrome::kChromeUIWelcomeURL);
  return use_later_run_variant
             ? net::AppendQueryParameter(url, "variant", "everywhere")
             : url;
}

#if defined(OS_WIN)
// static
GURL StartupTabProviderImpl::GetWin10WelcomePageUrl(
    bool use_later_run_variant) {
  GURL url(chrome::kChromeUIWelcomeWin10URL);
  return use_later_run_variant
             ? net::AppendQueryParameter(url, "text", "faster")
             : url;
}
#endif

// static
GURL StartupTabProviderImpl::GetTriggeredResetSettingsUrl() {
  return GURL(
      chrome::GetSettingsUrl(chrome::kTriggeredResetProfileSettingsSubPage));
}
