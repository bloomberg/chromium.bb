// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/static_cookie_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

class CookieSettingsTest : public testing::Test {
 public:
  CookieSettingsTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        cookie_settings_(CookieSettings::Factory::GetForProfile(&profile_)
                             .get()),
        kBlockedSite("http://ads.thirdparty.com"),
        kAllowedSite("http://good.allays.com"),
        kFirstPartySite("http://cool.things.com"),
        kBlockedFirstPartySite("http://no.thirdparties.com"),
        kExtensionURL("chrome-extension://deadbeef"),
        kHttpsSite("https://example.com"),
        kAllHttpsSitesPattern(ContentSettingsPattern::FromString("https://*")) {
  }

 protected:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  TestingProfile profile_;
  CookieSettings* cookie_settings_;
  const GURL kBlockedSite;
  const GURL kAllowedSite;
  const GURL kFirstPartySite;
  const GURL kBlockedFirstPartySite;
  const GURL kExtensionURL;
  const GURL kHttpsSite;
  ContentSettingsPattern kAllHttpsSitesPattern;
};

TEST_F(CookieSettingsTest, CookiesBlockSingle) {
  cookie_settings_->SetCookieSetting(
      ContentSettingsPattern::FromURL(kBlockedSite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings_->IsReadingCookieAllowed(
      kBlockedSite, kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesBlockThirdParty) {
  profile_.GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_FALSE(cookie_settings_->IsReadingCookieAllowed(
      kBlockedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
  EXPECT_FALSE(cookie_settings_->IsSettingCookieAllowed(
      kBlockedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesAllowThirdParty) {
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(
      kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(
      kBlockedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesExplicitBlockSingleThirdParty) {
  cookie_settings_->SetCookieSetting(
      ContentSettingsPattern::FromURL(kBlockedSite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings_->IsReadingCookieAllowed(
      kBlockedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsSettingCookieAllowed(
      kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(
      kAllowedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesExplicitSessionOnly) {
  cookie_settings_->SetCookieSetting(
      ContentSettingsPattern::FromURL(kBlockedSite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(
      kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(
      kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));

  profile_.GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_TRUE(cookie_settings_->
              IsReadingCookieAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->
              IsSettingCookieAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesThirdPartyBlockedExplicitAllow) {
  cookie_settings_->SetCookieSetting(
      ContentSettingsPattern::FromURL(kAllowedSite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_ALLOW);
  profile_.GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // Extensions should always be allowed to use cookies.
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(
      kAllowedSite, kExtensionURL));
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(
      kAllowedSite, kExtensionURL));

  // Extensions should always be allowed to use cookies.
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(
      kAllowedSite, kExtensionURL));
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(
      kAllowedSite, kExtensionURL));
}

TEST_F(CookieSettingsTest, CookiesThirdPartyBlockedAllSitesAllowed) {
  cookie_settings_->SetCookieSetting(
      ContentSettingsPattern::FromURL(kAllowedSite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_ALLOW);
  profile_.GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  // As an example for a pattern that matches all hosts but not all origins,
  // match all HTTPS sites.
  cookie_settings_->SetCookieSetting(
      kAllHttpsSitesPattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_ALLOW);
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);

  // |kAllowedSite| should be allowed.
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(
      kAllowedSite, kBlockedSite));
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(
      kAllowedSite, kBlockedSite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // HTTPS sites should be allowed in a first-party context.
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(
      kHttpsSite, kHttpsSite));
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(
      kHttpsSite, kHttpsSite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // HTTP sites should be allowed, but session-only.
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(
      kFirstPartySite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(
      kFirstPartySite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kFirstPartySite));

  // Third-party cookies should be blocked.
  EXPECT_FALSE(cookie_settings_->IsReadingCookieAllowed(
      kFirstPartySite, kBlockedSite));
  EXPECT_FALSE(cookie_settings_->IsSettingCookieAllowed(
      kFirstPartySite, kBlockedSite));
  EXPECT_FALSE(cookie_settings_->IsReadingCookieAllowed(
      kHttpsSite, kBlockedSite));
  EXPECT_FALSE(cookie_settings_->IsSettingCookieAllowed(
      kHttpsSite, kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesBlockEverything) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(cookie_settings_->IsReadingCookieAllowed(
      kFirstPartySite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsSettingCookieAllowed(
      kFirstPartySite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsSettingCookieAllowed(
      kAllowedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesBlockEverythingExceptAllowed) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(
      ContentSettingsPattern::FromURL(kAllowedSite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(cookie_settings_->IsReadingCookieAllowed(
      kFirstPartySite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsSettingCookieAllowed(
      kFirstPartySite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(
      kAllowedSite, kAllowedSite));
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(
      kAllowedSite, kAllowedSite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));
}

TEST_F(CookieSettingsTest, CookiesBlockSingleFirstParty) {
  cookie_settings_->SetCookieSetting(
      ContentSettingsPattern::FromURL(kAllowedSite),
      ContentSettingsPattern::FromURL(kFirstPartySite),
      CONTENT_SETTING_ALLOW);
  cookie_settings_->SetCookieSetting(
      ContentSettingsPattern::FromURL(kAllowedSite),
      ContentSettingsPattern::FromURL(kBlockedFirstPartySite),
      CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  EXPECT_FALSE(cookie_settings_->IsReadingCookieAllowed(
      kAllowedSite, kBlockedFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsSettingCookieAllowed(
      kAllowedSite, kBlockedFirstPartySite));

  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  EXPECT_FALSE(cookie_settings_->IsReadingCookieAllowed(
      kAllowedSite, kBlockedFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsSettingCookieAllowed(
      kAllowedSite, kBlockedFirstPartySite));

  cookie_settings_->ResetCookieSetting(
      ContentSettingsPattern::FromURL(kAllowedSite),
      ContentSettingsPattern::FromURL(kFirstPartySite));

  EXPECT_FALSE(cookie_settings_->IsReadingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings_->IsSettingCookieAllowed(
      kAllowedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, ExtensionsRegularSettings) {
  cookie_settings_->SetCookieSetting(
      ContentSettingsPattern::FromURL(kBlockedSite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_BLOCK);

  // Regular cookie settings also apply to extensions.
  EXPECT_FALSE(cookie_settings_->IsReadingCookieAllowed(
      kBlockedSite, kExtensionURL));
}

TEST_F(CookieSettingsTest, ExtensionsOwnCookies) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  // Extensions can always use cookies (and site data) in their own origin.
  EXPECT_TRUE(cookie_settings_->IsReadingCookieAllowed(
      kExtensionURL, kExtensionURL));
}

TEST_F(CookieSettingsTest, ExtensionsThirdParty) {
  profile_.GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, true);

  // XHRs stemming from extensions are exempt from third-party cookie blocking
  // rules (as the first party is always the extension's security origin).
  EXPECT_TRUE(cookie_settings_->IsSettingCookieAllowed(
      kBlockedSite, kExtensionURL));
}

}  // namespace
