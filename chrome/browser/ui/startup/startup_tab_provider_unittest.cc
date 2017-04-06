// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_tab_provider.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(StartupTabProviderTest, GetStandardOnboardingTabsForState) {
  // Show welcome page to new unauthenticated profile on first run.
  StartupTabs output =
      StartupTabProviderImpl::GetStandardOnboardingTabsForState(
          true, false, true, false, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[0].url);
  EXPECT_FALSE(output[0].is_pinned);

  // After first run, display welcome page using variant view.
  output = StartupTabProviderImpl::GetStandardOnboardingTabsForState(
      false, false, true, false, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(true), output[0].url);
  EXPECT_FALSE(output[0].is_pinned);
}

TEST(StartupTabProviderTest, GetStandardOnboardingTabsForState_Negative) {
  // Do not show the welcome page to the same profile twice.
  StartupTabs output =
      StartupTabProviderImpl::GetStandardOnboardingTabsForState(
          true, true, true, false, false);

  EXPECT_TRUE(output.empty());

  // Do not show the welcome page to authenticated users.
  output = StartupTabProviderImpl::GetStandardOnboardingTabsForState(
      true, false, true, true, false);

  EXPECT_TRUE(output.empty());

  // Do not show the welcome page if sign-in is disabled.
  output = StartupTabProviderImpl::GetStandardOnboardingTabsForState(
      true, false, false, false, false);

  EXPECT_TRUE(output.empty());

  // Do not show the welcome page to supervised users.
  output = StartupTabProviderImpl::GetStandardOnboardingTabsForState(
      true, false, true, false, true);

  EXPECT_TRUE(output.empty());
}

#if defined(OS_WIN)
TEST(StartupTabProviderTest, GetWin10OnboardingTabsForState) {
  // Show Win 10 Welcome page if it has not been seen, but the standard page
  // has.
  StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
      true, true, false, true, false, true, false, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWin10WelcomePageUrl(false),
            output[0].url);
  EXPECT_FALSE(output[0].is_pinned);

  // Show standard Welcome page if the Win 10 Welcome page has been seen, but
  // the standard page has not.
  output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
      true, false, true, true, false, true, false, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[0].url);
  EXPECT_FALSE(output[0].is_pinned);

  // If neither page has been seen, the Win 10 Welcome page takes precedence
  // this launch.
  output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
      true, false, false, true, false, true, false, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWin10WelcomePageUrl(false),
            output[0].url);
  EXPECT_FALSE(output[0].is_pinned);
}

TEST(StartupTabProviderTest, GetWin10OnboardingTabsForState_LaterRunVariant) {
  // Show a variant of the Win 10 Welcome page after first run, if it has not
  // been seen.
  StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
      false, false, false, true, false, true, false, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWin10WelcomePageUrl(true),
            output[0].url);
  EXPECT_FALSE(output[0].is_pinned);

  // Show a variant of the standard Welcome page after first run, if the Win 10
  // Welcome page has already been seen but the standard has not.
  output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
      false, false, true, true, false, true, false, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(true), output[0].url);
  EXPECT_FALSE(output[0].is_pinned);
}

TEST(StartupTabProviderTest, GetWin10OnboardingTabsForState_Negative) {
  // Do not show either page if it has already been shown.
  StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
      true, true, true, true, false, true, false, false);

  EXPECT_TRUE(output.empty());

  // Do not show either page to supervised users.
  output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
      true, false, false, true, false, true, false, true);

  EXPECT_TRUE(output.empty());

  // If Chrome is already the default browser, don't show the Win 10 Welcome
  // page, and don't preempt the standard Welcome page.
  output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
      true, false, false, true, false, true, true, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[0].url);
  EXPECT_FALSE(output[0].is_pinned);

  // If the user is signed in, block showing the standard Welcome page.
  output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
      true, false, true, true, true, true, false, false);

  EXPECT_TRUE(output.empty());

  // If sign-in is disabled, block showing the standard Welcome page.
  output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
      true, false, true, false, false, true, false, false);

  EXPECT_TRUE(output.empty());
}

TEST(StartupTabProviderTest,
     GetWin10OnboardingTabsForState_SetDefaultBrowserNotAllowed) {
  // Skip the Win 10 promo if setting the default browser is not allowed.
  StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
      true, false, false, true, false, false, false, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[0].url);

  // After first run, no onboarding content is displayed when setting the
  // default browser is not allowed.
  output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
      true, true, false, true, false, false, false, false);

  EXPECT_TRUE(output.empty());
}

#endif

TEST(StartupTabProviderTest, GetMasterPrefsTabsForState) {
  std::vector<GURL> input = {GURL(base::ASCIIToUTF16("https://new_tab_page")),
                             GURL(base::ASCIIToUTF16("https://www.google.com")),
                             GURL(base::ASCIIToUTF16("https://welcome_page"))};

  StartupTabs output =
      StartupTabProviderImpl::GetMasterPrefsTabsForState(true, input);

  ASSERT_EQ(3U, output.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);
  EXPECT_FALSE(output[0].is_pinned);
  EXPECT_EQ(input[1], output[1].url);
  EXPECT_FALSE(output[1].is_pinned);
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[2].url);
  EXPECT_FALSE(output[2].is_pinned);
}

TEST(StartupTabProviderTest, GetMasterPrefsTabsForState_FirstRunOnly) {
  std::vector<GURL> input = {
      GURL(base::ASCIIToUTF16("https://www.google.com"))};

  StartupTabs output =
      StartupTabProviderImpl::GetMasterPrefsTabsForState(false, input);

  EXPECT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, GetResetTriggerTabsForState) {
  StartupTabs output =
      StartupTabProviderImpl::GetResetTriggerTabsForState(true);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetTriggeredResetSettingsUrl(),
            output[0].url);
  EXPECT_FALSE(output[0].is_pinned);
}

TEST(StartupTabProviderTest, GetResetTriggerTabsForState_Negative) {
  StartupTabs output =
      StartupTabProviderImpl::GetResetTriggerTabsForState(false);

  ASSERT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, GetPinnedTabsForState) {
  StartupTabs pinned = {StartupTab(GURL("https://www.google.com"), true)};
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  SessionStartupPref pref_urls(SessionStartupPref::Type::URLS);

  StartupTabs output = StartupTabProviderImpl::GetPinnedTabsForState(
      pref_default, pinned, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("www.google.com", output[0].url.host());

  output =
      StartupTabProviderImpl::GetPinnedTabsForState(pref_urls, pinned, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("www.google.com", output[0].url.host());
}

TEST(StartupTabProviderTest, GetPinnedTabsForState_Negative) {
  StartupTabs pinned = {StartupTab(GURL("https://www.google.com"), true)};
  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);

  // Session restore preference should block reading pinned tabs.
  StartupTabs output =
      StartupTabProviderImpl::GetPinnedTabsForState(pref_last, pinned, false);

  ASSERT_TRUE(output.empty());

  // Pinned tabs are not added when this profile already has a nonempty tabbed
  // browser open.
  output =
      StartupTabProviderImpl::GetPinnedTabsForState(pref_default, pinned, true);

  ASSERT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, GetPreferencesTabsForState) {
  SessionStartupPref pref(SessionStartupPref::Type::URLS);
  pref.urls = {GURL(base::ASCIIToUTF16("https://www.google.com"))};

  StartupTabs output =
      StartupTabProviderImpl::GetPreferencesTabsForState(pref, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("www.google.com", output[0].url.host());
}

TEST(StartupTabProviderTest, GetPreferencesTabsForState_WrongType) {
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  pref_default.urls = {GURL(base::ASCIIToUTF16("https://www.google.com"))};

  StartupTabs output =
      StartupTabProviderImpl::GetPreferencesTabsForState(pref_default, false);

  EXPECT_TRUE(output.empty());

  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);
  pref_last.urls = {GURL(base::ASCIIToUTF16("https://www.google.com"))};

  output = StartupTabProviderImpl::GetPreferencesTabsForState(pref_last, false);

  EXPECT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, GetPreferencesTabsForState_NotFirstBrowser) {
  SessionStartupPref pref(SessionStartupPref::Type::URLS);
  pref.urls = {GURL(base::ASCIIToUTF16("https://www.google.com"))};

  StartupTabs output =
      StartupTabProviderImpl::GetPreferencesTabsForState(pref, true);

  EXPECT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, GetNewTabPageTabsForState) {
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  SessionStartupPref pref_urls(SessionStartupPref::Type::URLS);

  StartupTabs output =
      StartupTabProviderImpl::GetNewTabPageTabsForState(pref_default);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);

  output = StartupTabProviderImpl::GetNewTabPageTabsForState(pref_urls);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);
}

TEST(StartupTabProviderTest, GetNewTabPageTabsForState_Negative) {
  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);

  StartupTabs output =
      StartupTabProviderImpl::GetNewTabPageTabsForState(pref_last);

  ASSERT_TRUE(output.empty());
}
