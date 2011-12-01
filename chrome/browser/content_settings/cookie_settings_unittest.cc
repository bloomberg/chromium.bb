// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/static_cookie_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

class CookieSettingsTest : public testing::Test {
 public:
  CookieSettingsTest() :
      ui_thread_(BrowserThread::UI, &message_loop_),
      kBlockedSite("http://ads.thirdparty.com"),
      kAllowedSite("http://good.allays.com"),
      kFirstPartySite("http://cool.things.com"),
      kBlockedFirstPartySite("http://no.thirdparties.com"),
      kExtensionURL("chrome-extension://deadbeef") {}

 protected:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  const GURL kBlockedSite;
  const GURL kAllowedSite;
  const GURL kFirstPartySite;
  const GURL kBlockedFirstPartySite;
  const GURL kExtensionURL;
};

TEST_F(CookieSettingsTest, CookiesBlockSingle) {
  TestingProfile profile;
  CookieSettings* cookie_settings = CookieSettings::GetForProfile(&profile);
  cookie_settings->SetCookieSetting(
      ContentSettingsPattern::FromURL(kBlockedSite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings->IsReadingCookieAllowed(
      kBlockedSite, kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesBlockThirdParty) {
  TestingProfile profile;
  CookieSettings* cookie_settings = CookieSettings::GetForProfile(&profile);
  profile.GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_FALSE(cookie_settings->IsReadingCookieAllowed(
      kBlockedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings->IsCookieSessionOnly(kBlockedSite));
  EXPECT_FALSE(cookie_settings->IsSettingCookieAllowed(
      kBlockedSite, kFirstPartySite));

  CommandLine* cmd = CommandLine::ForCurrentProcess();
  AutoReset<CommandLine> auto_reset(cmd, *cmd);
  cmd->AppendSwitch(switches::kOnlyBlockSettingThirdPartyCookies);

  EXPECT_TRUE(cookie_settings->IsReadingCookieAllowed(
      kBlockedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings->IsSettingCookieAllowed(
      kBlockedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesAllowThirdParty) {
  TestingProfile profile;
  CookieSettings* cookie_settings = CookieSettings::GetForProfile(&profile);
  EXPECT_TRUE(cookie_settings->IsReadingCookieAllowed(
      kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      kBlockedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings->IsCookieSessionOnly(kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesExplicitBlockSingleThirdParty) {
  TestingProfile profile;
  CookieSettings* cookie_settings = CookieSettings::GetForProfile(&profile);
  cookie_settings->SetCookieSetting(
      ContentSettingsPattern::FromURL(kBlockedSite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings->IsReadingCookieAllowed(
      kBlockedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings->IsSettingCookieAllowed(
      kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      kAllowedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesExplicitSessionOnly) {
  TestingProfile profile;
  CookieSettings* cookie_settings = CookieSettings::GetForProfile(&profile);
  cookie_settings->SetCookieSetting(
      ContentSettingsPattern::FromURL(kBlockedSite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(cookie_settings->IsReadingCookieAllowed(
      kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings->IsCookieSessionOnly(kBlockedSite));

  profile.GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_TRUE(cookie_settings->
              IsReadingCookieAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings->
              IsSettingCookieAllowed(kBlockedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings->IsCookieSessionOnly(kBlockedSite));
}

TEST_F(CookieSettingsTest, CookiesThirdPartyBlockedExplicitAllow) {
  TestingProfile profile;
  CookieSettings* cookie_settings = CookieSettings::GetForProfile(&profile);
  cookie_settings->SetCookieSetting(
      ContentSettingsPattern::FromURL(kAllowedSite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_ALLOW);
  profile.GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_TRUE(cookie_settings->IsReadingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings->IsCookieSessionOnly(kAllowedSite));

  // Extensions should always be allowed to use cookies.
  EXPECT_TRUE(cookie_settings->IsReadingCookieAllowed(
      kAllowedSite, kExtensionURL));
  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      kAllowedSite, kExtensionURL));

  // Extensions should always be allowed to use cookies.
  EXPECT_TRUE(cookie_settings->IsReadingCookieAllowed(
      kAllowedSite, kExtensionURL));
  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      kAllowedSite, kExtensionURL));
}

TEST_F(CookieSettingsTest, CookiesBlockEverything) {
  TestingProfile profile;
  CookieSettings* cookie_settings = CookieSettings::GetForProfile(&profile);
  cookie_settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(cookie_settings->IsReadingCookieAllowed(
      kFirstPartySite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings->IsSettingCookieAllowed(
      kFirstPartySite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings->IsSettingCookieAllowed(
      kAllowedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, CookiesBlockEverythingExceptAllowed) {
  TestingProfile profile;
  CookieSettings* cookie_settings = CookieSettings::GetForProfile(&profile);
  cookie_settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings->SetCookieSetting(
      ContentSettingsPattern::FromURL(kAllowedSite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(cookie_settings->IsReadingCookieAllowed(
      kFirstPartySite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings->IsSettingCookieAllowed(
      kFirstPartySite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings->IsReadingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings->IsReadingCookieAllowed(
      kAllowedSite, kAllowedSite));
  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      kAllowedSite, kAllowedSite));
  EXPECT_FALSE(cookie_settings->IsCookieSessionOnly(kAllowedSite));
}

TEST_F(CookieSettingsTest, CookiesBlockSingleFirstParty) {
  TestingProfile profile;
  CookieSettings* cookie_settings = CookieSettings::GetForProfile(&profile);
  cookie_settings->SetCookieSetting(
      ContentSettingsPattern::FromURL(kAllowedSite),
      ContentSettingsPattern::FromURL(kFirstPartySite),
      CONTENT_SETTING_ALLOW);
  cookie_settings->SetCookieSetting(
      ContentSettingsPattern::FromURL(kAllowedSite),
      ContentSettingsPattern::FromURL(kBlockedFirstPartySite),
      CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(cookie_settings->IsReadingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings->IsCookieSessionOnly(kAllowedSite));

  EXPECT_FALSE(cookie_settings->IsReadingCookieAllowed(
      kAllowedSite, kBlockedFirstPartySite));
  EXPECT_FALSE(cookie_settings->IsSettingCookieAllowed(
      kAllowedSite, kBlockedFirstPartySite));

  cookie_settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(cookie_settings->IsReadingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings->IsCookieSessionOnly(kAllowedSite));

  EXPECT_FALSE(cookie_settings->IsReadingCookieAllowed(
      kAllowedSite, kBlockedFirstPartySite));
  EXPECT_FALSE(cookie_settings->IsSettingCookieAllowed(
      kAllowedSite, kBlockedFirstPartySite));

  cookie_settings->ResetCookieSetting(
      ContentSettingsPattern::FromURL(kAllowedSite),
      ContentSettingsPattern::FromURL(kFirstPartySite));

  EXPECT_FALSE(cookie_settings->IsReadingCookieAllowed(
      kAllowedSite, kFirstPartySite));
  EXPECT_FALSE(cookie_settings->IsSettingCookieAllowed(
      kAllowedSite, kFirstPartySite));
}

TEST_F(CookieSettingsTest, ExtensionsRegularSettings) {
  TestingProfile profile;
  CookieSettings* cookie_settings = CookieSettings::GetForProfile(&profile);
  cookie_settings->SetCookieSetting(
      ContentSettingsPattern::FromURL(kBlockedSite),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTING_BLOCK);

  // Regular cookie settings also apply to extensions.
  EXPECT_FALSE(cookie_settings->IsReadingCookieAllowed(
      kBlockedSite, kExtensionURL));
}

TEST_F(CookieSettingsTest, ExtensionsOwnCookies) {
  TestingProfile profile;
  CookieSettings* cookie_settings = CookieSettings::GetForProfile(&profile);
  cookie_settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  // Extensions can always use cookies (and site data) in their own origin.
  EXPECT_TRUE(cookie_settings->IsReadingCookieAllowed(
      kExtensionURL, kExtensionURL));
}

TEST_F(CookieSettingsTest, ExtensionsThirdParty) {
  TestingProfile profile;
  CookieSettings* cookie_settings = CookieSettings::GetForProfile(&profile);
  profile.GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, true);

  // XHRs stemming from extensions are exempt from third-party cookie blocking
  // rules (as the first party is always the extension's security origin).
  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      kBlockedSite, kExtensionURL));
}

}  // namespace
