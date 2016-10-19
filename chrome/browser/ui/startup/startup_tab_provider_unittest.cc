// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_tab_provider.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(StartupTabProviderTest, CheckStandardOnboardingTabPolicy) {
  StartupTabs output =
      StartupTabProviderImpl::CheckStandardOnboardingTabPolicy(true);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(), output[0].url);
  EXPECT_FALSE(output[0].is_pinned);
}

TEST(StartupTabProviderTest, CheckStandardOnboardingTabPolicy_FirstRunOnly) {
  StartupTabs output =
      StartupTabProviderImpl::CheckStandardOnboardingTabPolicy(false);

  EXPECT_TRUE(output.empty());
}

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
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(), output[2].url);
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
