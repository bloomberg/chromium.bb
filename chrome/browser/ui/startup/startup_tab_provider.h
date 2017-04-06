// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_PROVIDER_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_PROVIDER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "url/gurl.h"

// Provides the sets of tabs to be shown at startup for given sets of policy.
// For instance, this class answers the question, "which tabs, if any, need to
// be shown for first run/onboarding?" Provided as a virtual interface to allow
// faking in unit tests.
class StartupTabProvider {
 public:
  // Gathers relevant system state and returns any tabs which should be
  // shown according to onboarding/first run policy.
  virtual StartupTabs GetOnboardingTabs(Profile* profile) const = 0;

  // Gathers URLs from a Master Preferences file indicating first run logic
  // specific to this distribution. Transforms any such URLs per policy and
  // returns them. Also clears the value of first_run_urls_ in the provided
  // BrowserCreator.
  virtual StartupTabs GetDistributionFirstRunTabs(
      StartupBrowserCreator* browser_creator) const = 0;

  // Checks for the presence of a trigger indicating the need to offer a Profile
  // Reset on this profile. Returns any tabs which should be shown accordingly.
  virtual StartupTabs GetResetTriggerTabs(Profile* profile) const = 0;

  // Returns the user's pinned tabs, if they should be shown.
  virtual StartupTabs GetPinnedTabs(const base::CommandLine& command_line,
                                    Profile* profile) const = 0;

  // Returns tabs, if any, specified in the user's preferences as the default
  // content for a new window.
  virtual StartupTabs GetPreferencesTabs(const base::CommandLine& command_line,
                                         Profile* profile) const = 0;

  // Returns the New Tab Page, if the user's preferences indicate a
  // configuration where it must be passed explicitly.
  virtual StartupTabs GetNewTabPageTabs(const base::CommandLine& command_line,
                                        Profile* profile) const = 0;
};

class StartupTabProviderImpl : public StartupTabProvider {
 public:
  StartupTabProviderImpl() = default;

  // The static helper methods below implement the policies relevant to the
  // respective Get*Tabs methods, but do not gather or interact with any
  // system state relating to making those policy decisions. Exposed for
  // testing.

  // Determines which tabs should be shown according to onboarding/first
  // run policy.
  static StartupTabs GetStandardOnboardingTabsForState(
      bool is_first_run,
      bool has_seen_welcome_page,
      bool is_signin_allowed,
      bool is_signed_in,
      bool is_supervised_user);

#if defined(OS_WIN)
  // Determines which tabs should be shown according to onboarding/first run
  // policy, including promo content specific to Windows 10.
  static StartupTabs GetWin10OnboardingTabsForState(
      bool is_first_run,
      bool has_seen_welcome_page,
      bool has_seen_win10_promo,
      bool is_signin_allowed,
      bool is_signed_in,
      bool set_default_browser_allowed,
      bool is_default_browser,
      bool is_supervised_user);
#endif

  // Processes first run URLs specified in Master Preferences file, replacing
  // any "magic word" URL hosts with appropriate URLs.
  static StartupTabs GetMasterPrefsTabsForState(
      bool is_first_run,
      const std::vector<GURL>& first_run_tabs);

  // Determines which tabs should be shown as a result of the presence/absence
  // of a Reset Trigger on this profile.
  static StartupTabs GetResetTriggerTabsForState(bool profile_has_trigger);

  // Determines whether the startup preference requires the contents of
  // |pinned_tabs| to be shown. This is needed to avoid duplicates, as the
  // session restore logic will also resurface pinned tabs on its own.
  static StartupTabs GetPinnedTabsForState(
      const SessionStartupPref& pref,
      const StartupTabs& pinned_tabs,
      bool profile_has_other_tabbed_browser);

  // Determines whether preferences and window state indicate that
  // user-specified tabs should be shown as the default new window content, and
  // returns the specified tabs if so.
  static StartupTabs GetPreferencesTabsForState(
      const SessionStartupPref& pref,
      bool profile_has_other_tabbed_browser);

  // Determines whether startup preferences require the New Tab Page to be
  // explicitly specified. Session Restore does not expect the NTP to be passed.
  static StartupTabs GetNewTabPageTabsForState(const SessionStartupPref& pref);

  // Gets the URL for the Welcome page. If |use_later_run_variant| is true, a
  // URL parameter will be appended so as to access the variant page used when
  // onboarding occurs after the first Chrome execution (e.g., when creating an
  // additional profile).
  static GURL GetWelcomePageUrl(bool use_later_run_variant);

#if defined(OS_WIN)
  // Gets the URL for the Windows 10 Welcome page. If |use_later_run_variant| is
  // true, a URL parameter will be appended so as to access the variant page
  // used when onboarding occurs after the first Chrome execution.
  static GURL GetWin10WelcomePageUrl(bool use_later_run_variant);
#endif

  // Gets the URL for the page which offers to reset the user's profile
  // settings.
  static GURL GetTriggeredResetSettingsUrl();

  // StartupTabProvider:
  StartupTabs GetOnboardingTabs(Profile* profile) const override;
  StartupTabs GetDistributionFirstRunTabs(
      StartupBrowserCreator* browser_creator) const override;
  StartupTabs GetResetTriggerTabs(Profile* profile) const override;
  StartupTabs GetPinnedTabs(const base::CommandLine& command_line,
                            Profile* profile) const override;
  StartupTabs GetPreferencesTabs(const base::CommandLine& command_line,
                                 Profile* profile) const override;
  StartupTabs GetNewTabPageTabs(const base::CommandLine& command_line,
                                Profile* profile) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StartupTabProviderImpl);
};

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_PROVIDER_H_
