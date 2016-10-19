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
  virtual StartupTabs GetOnboardingTabs() const = 0;

  // Gathers URLs from a Master Preferences file indicating first run logic
  // specific to this distribution. Transforms any such URLs per policy and
  // returns them. Also clears the value of first_run_urls_ in the provided
  // BrowserCreator.
  virtual StartupTabs GetDistributionFirstRunTabs(
      StartupBrowserCreator* browser_creator) const = 0;

  // Checks for the presence of a trigger indicating the need to offer a Profile
  // Reset on this profile. Returns any tabs which should be shown accordingly.
  virtual StartupTabs GetResetTriggerTabs(Profile* profile) const = 0;

  // Returns the user's pinned tabs.
  virtual StartupTabs GetPinnedTabs(Profile* profile) const = 0;

  // Returns tabs, if any, specified in the user's preferences as the default
  // content for a new window.
  virtual StartupTabs GetPreferencesTabs(const base::CommandLine& command_line,
                                         Profile* profile) const = 0;
};

class StartupTabProviderImpl : public StartupTabProvider {
 public:
  StartupTabProviderImpl() = default;

  // The static Check*TabPolicy methods below enforce the policies relevant to
  // the respective Get*Tabs methods, but do not gather or interact with any
  // system state relating to making those policy decisions.

  // Determines which tabs which should be shown according to onboarding/first
  // run policy.
  static StartupTabs CheckStandardOnboardingTabPolicy(bool is_first_run);

  // Processes first run URLs specified in Master Preferences file, replacing
  // any "magic word" URL hosts with appropriate URLs.
  static StartupTabs CheckMasterPrefsTabPolicy(
      bool is_first_run,
      const std::vector<GURL>& first_run_tabs);

  // Determines which tabs should be shown as a result of the presence/absence
  // of a Reset Trigger on this profile.
  static StartupTabs CheckResetTriggerTabPolicy(bool profile_has_trigger);

  // Determines whether preferences indicate that user-specified tabs should be
  // shown as the default new window content, and returns the specified tabs if
  // so.
  static StartupTabs CheckPreferencesTabPolicy(SessionStartupPref pref);

  // Gets the URL for the "Welcome to Chrome" page.
  static GURL GetWelcomePageUrl();

  // Gets the URL for the page which offers to reset the user's profile
  // settings.
  static GURL GetTriggeredResetSettingsUrl();

  // StartupTabProvider:
  StartupTabs GetOnboardingTabs() const override;
  StartupTabs GetDistributionFirstRunTabs(
      StartupBrowserCreator* browser_creator) const override;
  StartupTabs GetResetTriggerTabs(Profile* profile) const override;
  StartupTabs GetPinnedTabs(Profile* profile) const override;
  StartupTabs GetPreferencesTabs(const base::CommandLine& command_line,
                                 Profile* profile) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StartupTabProviderImpl);
};

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_PROVIDER_H_
