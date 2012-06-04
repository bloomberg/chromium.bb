// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "net/cookies/cookie_monster.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::BrowserThread;

namespace {

class MockSiteDataObserver
    : public TabSpecificContentSettings::SiteDataObserver {
 public:
  explicit MockSiteDataObserver(
      TabSpecificContentSettings* tab_specific_content_settings)
      : SiteDataObserver(tab_specific_content_settings) {
  }

  virtual ~MockSiteDataObserver() {}

  MOCK_METHOD0(OnSiteDataAccessed, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSiteDataObserver);
};

}  // namespace

class TabSpecificContentSettingsTest : public ChromeRenderViewHostTestHarness {
 public:
  TabSpecificContentSettingsTest()
      : browser_thread_(BrowserThread::UI, &message_loop_) {}

 private:
  content::TestBrowserThread browser_thread_;

  DISALLOW_COPY_AND_ASSIGN(TabSpecificContentSettingsTest);
};

TEST_F(TabSpecificContentSettingsTest, BlockedContent) {
  TabSpecificContentSettings content_settings(contents());
  net::CookieOptions options;

  // Check that after initializing, nothing is blocked.
  EXPECT_FALSE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_FALSE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));

  // Set a cookie, block access to images, block a popup.
  content_settings.OnCookieChanged(GURL("http://google.com"),
                                   GURL("http://google.com"),
                                   "A=B",
                                   options,
                                   false);
  content_settings.OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES,
                                    std::string());
  content_settings.SetPopupsBlocked(true);

  // Check that only the respective content types are affected.
  EXPECT_TRUE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_TRUE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));
  content_settings.OnCookieChanged(GURL("http://google.com"),
                                   GURL("http://google.com"),
                                   "A=B",
                                   options,
                                   false);

  // Block a cookie.
  content_settings.OnCookieChanged(GURL("http://google.com"),
                                   GURL("http://google.com"),
                                   "C=D",
                                   options,
                                   true);
  EXPECT_TRUE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));

  // Reset blocked content settings.
  content_settings.ClearBlockedContentSettingsExceptForCookies();
  EXPECT_FALSE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  EXPECT_TRUE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_FALSE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));

  content_settings.ClearCookieSpecificContentSettings();
  EXPECT_FALSE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_FALSE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));
}

TEST_F(TabSpecificContentSettingsTest, BlockedFileSystems) {
  TabSpecificContentSettings content_settings(contents());

  // Access a file system.
  content_settings.OnFileSystemAccessed(GURL("http://google.com"), false);
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));

  // Block access to a file system.
  content_settings.OnFileSystemAccessed(GURL("http://google.com"), true);
  EXPECT_TRUE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}

TEST_F(TabSpecificContentSettingsTest, AllowedContent) {
  TabSpecificContentSettings content_settings(contents());
  net::CookieOptions options;

  ASSERT_FALSE(
      content_settings.IsContentAccessed(CONTENT_SETTINGS_TYPE_IMAGES));
  ASSERT_FALSE(
      content_settings.IsContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  content_settings.OnCookieChanged(GURL("http://google.com"),
                                   GURL("http://google.com"),
                                   "A=B",
                                   options,
                                   false);
  ASSERT_TRUE(
      content_settings.IsContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  content_settings.OnCookieChanged(GURL("http://google.com"),
                                   GURL("http://google.com"),
                                   "C=D",
                                   options,
                                   true);
  ASSERT_TRUE(
      content_settings.IsContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_TRUE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}

TEST_F(TabSpecificContentSettingsTest, EmptyCookieList) {
  TabSpecificContentSettings content_settings(contents());

  ASSERT_FALSE(
      content_settings.IsContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  content_settings.OnCookiesRead(GURL("http://google.com"),
                                 GURL("http://google.com"),
                                 net::CookieList(),
                                 true);
  ASSERT_FALSE(
      content_settings.IsContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}

TEST_F(TabSpecificContentSettingsTest, SiteDataObserver) {
  TabSpecificContentSettings content_settings(contents());
  MockSiteDataObserver mock_observer(&content_settings);
  EXPECT_CALL(mock_observer, OnSiteDataAccessed()).Times(6);

  bool blocked_by_policy = false;
  content_settings.OnCookieChanged(GURL("http://google.com"),
                                   GURL("http://google.com"),
                                   "A=B",
                                   net::CookieOptions(),
                                   blocked_by_policy);
  net::CookieList cookie_list;
  net::CookieMonster::ParsedCookie parsed_cookie("CookieName=CookieValue");
  scoped_ptr<net::CookieMonster::CanonicalCookie> cookie(
      net::CookieMonster::CanonicalCookie::Create(GURL("http://google.com"),
                                                  parsed_cookie));
  cookie_list.push_back(*cookie);
  content_settings.OnCookiesRead(GURL("http://google.com"),
                                 GURL("http://google.com"),
                                 cookie_list,
                                 blocked_by_policy);
  content_settings.OnFileSystemAccessed(GURL("http://google.com"),
                                             blocked_by_policy);
  content_settings.OnIndexedDBAccessed(GURL("http://google.com"),
                                       UTF8ToUTF16("text"),
                                       blocked_by_policy);
  content_settings.OnLocalStorageAccessed(GURL("http://google.com"),
                                          true,
                                          blocked_by_policy);
  content_settings.OnWebDatabaseAccessed(GURL("http://google.com"),
                                         UTF8ToUTF16("name"),
                                         UTF8ToUTF16("display_name"),
                                         blocked_by_policy);
}
