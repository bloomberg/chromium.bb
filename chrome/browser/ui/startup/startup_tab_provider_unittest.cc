// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_tab_provider.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(StartupTabProviderTest, CheckStandardOnboardingTabPolicy) {
  // Show welcome page to new unauthenticated profile on first run.
  StartupTabs output = StartupTabProviderImpl::CheckStandardOnboardingTabPolicy(
      true, false, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[0].url);
  EXPECT_FALSE(output[0].is_pinned);

  // After first run, display welcome page using variant view.
  output = StartupTabProviderImpl::CheckStandardOnboardingTabPolicy(
      false, false, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(true), output[0].url);
  EXPECT_FALSE(output[0].is_pinned);
}

TEST(StartupTabProviderTest, CheckStandardOnboardingTabPolicy_Negative) {
  // Do not show the welcome page to the same profile twice.
  StartupTabs output = StartupTabProviderImpl::CheckStandardOnboardingTabPolicy(
      true, true, false);

  EXPECT_TRUE(output.empty());

  // Do not show the welcome page to authenticated users.
  output = StartupTabProviderImpl::CheckStandardOnboardingTabPolicy(true, false,
                                                                    true);

  EXPECT_TRUE(output.empty());
}

#if defined(OS_WIN)
TEST(StartupTabProviderTest, CheckWin10OnboardingTabPolicy) {
  // Show Win 10 Welcome page if it has not been seen, but the standard page
  // has.
  StartupTabs output = StartupTabProviderImpl::CheckWin10OnboardingTabPolicy(
      true, true, false, false, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWin10WelcomePageUrl(false),
            output[0].url);
  EXPECT_FALSE(output[0].is_pinned);

  // Show standard Welcome page if the Win 10 Welcome page has been seen, but
  // the standard page has not.
  output = StartupTabProviderImpl::CheckWin10OnboardingTabPolicy(
      true, false, true, false, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[0].url);
  EXPECT_FALSE(output[0].is_pinned);

  // If neither page has been seen, the Win 10 Welcome page takes precedence
  // this launch.
  output = StartupTabProviderImpl::CheckWin10OnboardingTabPolicy(
      true, false, false, false, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWin10WelcomePageUrl(false),
            output[0].url);
  EXPECT_FALSE(output[0].is_pinned);
}

TEST(StartupTabProviderTest, CheckWin10OnboardingTabPolicy_LaterRunVariant) {
  // Show a variant of the Win 10 Welcome page after first run, if it has not
  // been seen.
  StartupTabs output = StartupTabProviderImpl::CheckWin10OnboardingTabPolicy(
      false, false, false, false, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWin10WelcomePageUrl(true),
            output[0].url);
  EXPECT_FALSE(output[0].is_pinned);

  // Show a variant of the standard Welcome page after first run, if the Win 10
  // Welcome page has already been seen but the standard has not.
  output = StartupTabProviderImpl::CheckWin10OnboardingTabPolicy(
      false, false, true, false, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(true), output[0].url);
  EXPECT_FALSE(output[0].is_pinned);
}

TEST(StartupTabProviderTest, CheckWin10OnboardingTabPolicy_Negative) {
  // Do not show either page if it has already been shown.
  StartupTabs output = StartupTabProviderImpl::CheckWin10OnboardingTabPolicy(
      true, true, true, false, false);

  EXPECT_TRUE(output.empty());

  // If Chrome is already the default browser, don't show the Win 10 Welcome
  // page, and don't preempt the standard Welcome page.
  output = StartupTabProviderImpl::CheckWin10OnboardingTabPolicy(
      true, false, false, false, true);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[0].url);
  EXPECT_FALSE(output[0].is_pinned);

  // If the user is signed in, block showing the standard Welcome page.
  output = StartupTabProviderImpl::CheckWin10OnboardingTabPolicy(
      true, false, true, true, false);

  EXPECT_TRUE(output.empty());
}
#endif

TEST(StartupTabProviderTest, CheckMasterPrefsTabPolicy) {
  std::vector<GURL> input = {GURL(base::ASCIIToUTF16("https://new_tab_page")),
                             GURL(base::ASCIIToUTF16("https://www.google.com")),
                             GURL(base::ASCIIToUTF16("https://welcome_page"))};

  StartupTabs output =
      StartupTabProviderImpl::CheckMasterPrefsTabPolicy(true, input);

  ASSERT_EQ(3U, output.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);
  EXPECT_FALSE(output[0].is_pinned);
  EXPECT_EQ(input[1], output[1].url);
  EXPECT_FALSE(output[1].is_pinned);
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[2].url);
  EXPECT_FALSE(output[2].is_pinned);
}

TEST(StartupTabProviderTest, CheckMasterPrefsTabPolicy_FirstRunOnly) {
  std::vector<GURL> input = {
      GURL(base::ASCIIToUTF16("https://www.google.com"))};

  StartupTabs output =
      StartupTabProviderImpl::CheckMasterPrefsTabPolicy(false, input);

  EXPECT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, CheckResetTriggerTabPolicy) {
  StartupTabs output = StartupTabProviderImpl::CheckResetTriggerTabPolicy(true);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetTriggeredResetSettingsUrl(),
            output[0].url);
  EXPECT_FALSE(output[0].is_pinned);
}

TEST(StartupTabProviderTest, CheckResetTriggerTabPolicy_Negative) {
  StartupTabs output =
      StartupTabProviderImpl::CheckResetTriggerTabPolicy(false);

  ASSERT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, CheckPinnedTabPolicy) {
  StartupTabs pinned = {StartupTab(GURL("https://www.google.com"), true)};
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  SessionStartupPref pref_urls(SessionStartupPref::Type::URLS);

  StartupTabs output =
      StartupTabProviderImpl::CheckPinnedTabPolicy(pref_default, pinned);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("www.google.com", output[0].url.host());

  output = StartupTabProviderImpl::CheckPinnedTabPolicy(pref_urls, pinned);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("www.google.com", output[0].url.host());
}

TEST(StartupTabProviderTest, CheckPinnedTabPolicy_Negative) {
  StartupTabs pinned = {StartupTab(GURL("https://www.google.com"), true)};
  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);

  StartupTabs output =
      StartupTabProviderImpl::CheckPinnedTabPolicy(pref_last, pinned);

  ASSERT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, CheckPreferencesTabPolicy) {
  SessionStartupPref pref(SessionStartupPref::Type::URLS);
  pref.urls = {GURL(base::ASCIIToUTF16("https://www.google.com"))};

  StartupTabs output = StartupTabProviderImpl::CheckPreferencesTabPolicy(pref);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("www.google.com", output[0].url.host());
}

TEST(StartupTabProviderTest, CheckPreferencesTabPolicy_Negative) {
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  pref_default.urls = {GURL(base::ASCIIToUTF16("https://www.google.com"))};

  StartupTabs output =
      StartupTabProviderImpl::CheckPreferencesTabPolicy(pref_default);

  EXPECT_TRUE(output.empty());

  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);
  pref_last.urls = {GURL(base::ASCIIToUTF16("https://www.google.com"))};

  output = StartupTabProviderImpl::CheckPreferencesTabPolicy(pref_last);

  EXPECT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, CheckNewTabPageTabPolicy) {
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  SessionStartupPref pref_urls(SessionStartupPref::Type::URLS);

  StartupTabs output =
      StartupTabProviderImpl::CheckNewTabPageTabPolicy(pref_default);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);

  output = StartupTabProviderImpl::CheckNewTabPageTabPolicy(pref_urls);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);
}

TEST(StartupTabProviderTest, CheckNewTabPageTabPolicy_Negative) {
  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);

  StartupTabs output =
      StartupTabProviderImpl::CheckNewTabPageTabPolicy(pref_last);

  ASSERT_TRUE(output.empty());
}
