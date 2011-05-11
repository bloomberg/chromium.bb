// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_cookie_policy.h"

#include "chrome/browser/content_settings/content_settings_pattern.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

static const GURL kBlockedSite = GURL("http://ads.thirdparty.com");
static const GURL kAllowedSite = GURL("http://good.allays.com");
static const GURL kFirstPartySite = GURL("http://cool.things.com");

class ChromeCookiePolicyTest : public testing::Test {
 public:
  ChromeCookiePolicyTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_),
        cookie_policy_(settings()) {
  }

  TestingProfile* profile() { return &profile_; }

  HostContentSettingsMap* settings() {
    return profile()->GetHostContentSettingsMap();
  }

  ChromeCookiePolicy* policy() { return &cookie_policy_; }

  void set_strict_third_party_blocking(bool flag) {
    policy()->strict_third_party_blocking_ = flag;
  }

  void SetException(const GURL& url, ContentSetting setting) {
    settings()->AddExceptionForURL(url, CONTENT_SETTINGS_TYPE_COOKIES, "",
        setting);
  }

 private:
  // HostContentSettingsMap can only operate and be deleted on the UI thread.
  // Give it a fake one.
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  BrowserThread io_thread_;

  TestingProfile profile_;
  ChromeCookiePolicy cookie_policy_;
};

namespace {

TEST_F(ChromeCookiePolicyTest, BlockSingle) {
  SetException(kBlockedSite, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            policy()->CanGetCookies(kBlockedSite, kBlockedSite));
}

TEST_F(ChromeCookiePolicyTest, BlockThirdParty) {
  settings()->SetBlockThirdPartyCookies(true);
  EXPECT_EQ(net::OK,
            policy()->CanGetCookies(kBlockedSite, kFirstPartySite));
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            policy()->CanSetCookie(kBlockedSite, kFirstPartySite, ""));

  set_strict_third_party_blocking(true);
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            policy()->CanGetCookies(kBlockedSite, kFirstPartySite));
}

TEST_F(ChromeCookiePolicyTest, AllowThirdParty) {
  settings()->SetBlockThirdPartyCookies(false);
  EXPECT_EQ(net::OK, policy()->CanGetCookies(kBlockedSite, kFirstPartySite));
  EXPECT_EQ(net::OK, policy()->CanSetCookie(kBlockedSite, kFirstPartySite, ""));
}

TEST_F(ChromeCookiePolicyTest, ExplicitBlockSingleThirdParty) {
  SetException(kBlockedSite, CONTENT_SETTING_BLOCK);
  settings()->SetBlockThirdPartyCookies(false);

  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            policy()->CanGetCookies(kBlockedSite, kFirstPartySite));
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            policy()->CanSetCookie(kBlockedSite, kFirstPartySite, ""));
  EXPECT_EQ(net::OK,
            policy()->CanSetCookie(kAllowedSite, kFirstPartySite, ""));
}

TEST_F(ChromeCookiePolicyTest, ExplicitSessionOnly) {
  SetException(kBlockedSite, CONTENT_SETTING_SESSION_ONLY);

  settings()->SetBlockThirdPartyCookies(false);
  EXPECT_EQ(net::OK, policy()->CanGetCookies(kBlockedSite, kFirstPartySite));
  EXPECT_EQ(net::OK_FOR_SESSION_ONLY,
            policy()->CanSetCookie(kBlockedSite, kFirstPartySite, ""));

  settings()->SetBlockThirdPartyCookies(true);
  EXPECT_EQ(net::OK, policy()->CanGetCookies(kBlockedSite, kFirstPartySite));
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            policy()->CanSetCookie(kBlockedSite, kFirstPartySite, ""));
}

TEST_F(ChromeCookiePolicyTest, ThirdPartyAlwaysBlocked) {
  SetException(kAllowedSite, CONTENT_SETTING_ALLOW);

  settings()->SetBlockThirdPartyCookies(true);
  EXPECT_EQ(net::OK,
            policy()->CanGetCookies(kAllowedSite, kFirstPartySite));
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            policy()->CanSetCookie(kAllowedSite, kFirstPartySite, ""));

  set_strict_third_party_blocking(true);
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            policy()->CanGetCookies(kAllowedSite, kFirstPartySite));
}

TEST_F(ChromeCookiePolicyTest, BlockEverything) {
  settings()->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            policy()->CanGetCookies(kFirstPartySite, kFirstPartySite));
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            policy()->CanSetCookie(kFirstPartySite, kFirstPartySite, ""));
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            policy()->CanSetCookie(kAllowedSite, kFirstPartySite, ""));
}

TEST_F(ChromeCookiePolicyTest, BlockEverythingExceptAllowed) {
  settings()->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
      CONTENT_SETTING_BLOCK);
  SetException(kAllowedSite, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            policy()->CanGetCookies(kFirstPartySite, kFirstPartySite));
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            policy()->CanSetCookie(kFirstPartySite, kFirstPartySite, ""));
  EXPECT_EQ(net::OK, policy()->CanGetCookies(kAllowedSite, kFirstPartySite));
  EXPECT_EQ(net::OK, policy()->CanSetCookie(kAllowedSite, kFirstPartySite, ""));
  EXPECT_EQ(net::OK, policy()->CanGetCookies(kAllowedSite, kAllowedSite));
  EXPECT_EQ(net::OK, policy()->CanSetCookie(kAllowedSite, kAllowedSite, ""));
}

}
