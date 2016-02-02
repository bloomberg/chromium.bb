// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class CookieSettingsFactoryTest : public testing::Test {
 public:
  CookieSettingsFactoryTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        cookie_settings_(CookieSettingsFactory::GetForProfile(&profile_).get()),
        kBlockedSite("http://ads.thirdparty.com"),
        kAllowedSite("http://good.allays.com"),
        kFirstPartySite("http://cool.things.com"),
        kHttpsSite("https://example.com") {}

 protected:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  TestingProfile profile_;
  content_settings::CookieSettings* cookie_settings_;
  const GURL kBlockedSite;
  const GURL kAllowedSite;
  const GURL kFirstPartySite;
  const GURL kHttpsSite;
};

TEST_F(CookieSettingsFactoryTest, IncognitoBehaviorOfBlockingRules) {
  scoped_refptr<content_settings::CookieSettings> incognito_settings =
      CookieSettingsFactory::GetForProfile(profile_.GetOffTheRecordProfile());

  // Modify the regular cookie settings after the incognito cookie settings have
  // been instantiated.
  cookie_settings_->SetCookieSetting(
      ContentSettingsPattern::FromURL(kBlockedSite),
      ContentSettingsPattern::Wildcard(), CONTENT_SETTING_BLOCK);

  // The modification should apply to the regular profile and incognito profile.
  EXPECT_FALSE(
      cookie_settings_->IsReadingCookieAllowed(kBlockedSite, kBlockedSite));
  EXPECT_FALSE(
      incognito_settings->IsReadingCookieAllowed(kBlockedSite, kBlockedSite));

  // Modify an incognito cookie setting and check that this does not propagate
  // into regular mode.
  incognito_settings->SetCookieSetting(
      ContentSettingsPattern::FromURL(kHttpsSite),
      ContentSettingsPattern::Wildcard(), CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(kHttpsSite, kHttpsSite));
  EXPECT_FALSE(
      incognito_settings->IsReadingCookieAllowed(kHttpsSite, kHttpsSite));
}

TEST_F(CookieSettingsFactoryTest, IncognitoBehaviorOfBlockingEverything) {
  scoped_refptr<content_settings::CookieSettings> incognito_settings =
      CookieSettingsFactory::GetForProfile(profile_.GetOffTheRecordProfile());

  // Apply the general blocking to the regular profile.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  // It should be effective for regular and incognito session.
  EXPECT_FALSE(cookie_settings_->IsReadingCookieAllowed(kFirstPartySite,
                                                        kFirstPartySite));
  EXPECT_FALSE(incognito_settings->IsReadingCookieAllowed(kFirstPartySite,
                                                          kFirstPartySite));

  // A whitelisted item set in incognito mode should only apply to incognito
  // mode.
  incognito_settings->SetCookieSetting(
      ContentSettingsPattern::FromURL(kAllowedSite),
      ContentSettingsPattern::Wildcard(), CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(
      incognito_settings->IsReadingCookieAllowed(kAllowedSite, kAllowedSite));
  EXPECT_FALSE(
      cookie_settings_->IsReadingCookieAllowed(kAllowedSite, kAllowedSite));

  // A whitelisted item set in regular mode should apply to regular and
  // incognito mode.
  cookie_settings_->SetCookieSetting(
      ContentSettingsPattern::FromURL(kHttpsSite),
      ContentSettingsPattern::Wildcard(), CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(
      incognito_settings->IsReadingCookieAllowed(kHttpsSite, kHttpsSite));
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(kHttpsSite, kHttpsSite));
}

}  // namespace
