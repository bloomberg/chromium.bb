// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_specific_content_settings.h"

#include "chrome/test/testing_profile.h"
#include "net/base/cookie_monster.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class TestContentSettingsDelegate
    : public TabSpecificContentSettings::Delegate {
 public:
  TestContentSettingsDelegate()
      : settings_changed_(false), content_blocked_(false) {}
  virtual ~TestContentSettingsDelegate() {}

  void Reset() { settings_changed_ = content_blocked_ = false; }

  bool SettingsChanged() { return settings_changed_; }

  bool ContentBlocked() { return content_blocked_; }

  // TabSpecificContentSettings::Delegate implementation.
  virtual void OnContentSettingsAccessed(bool content_was_blocked) {
    settings_changed_ = true;
    content_blocked_ = content_was_blocked;
  }

 private:
  bool settings_changed_;
  bool content_blocked_;

  DISALLOW_COPY_AND_ASSIGN(TestContentSettingsDelegate);
};
}  // namespace

TEST(TabSpecificContentSettingsTest, BlockedContent) {
  TestContentSettingsDelegate test_delegate;
  TestingProfile profile;
  TabSpecificContentSettings content_settings(&test_delegate, &profile);
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
  content_settings.OnCookieChanged(
      GURL("http://google.com"), "A=B", options, false);
  EXPECT_TRUE(test_delegate.SettingsChanged());
  EXPECT_FALSE(test_delegate.ContentBlocked());
  test_delegate.Reset();
  content_settings.OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES,
                                    std::string());
  EXPECT_TRUE(test_delegate.SettingsChanged());
  EXPECT_TRUE(test_delegate.ContentBlocked());
  test_delegate.Reset();
  content_settings.SetPopupsBlocked(true);
  EXPECT_TRUE(test_delegate.SettingsChanged());
  EXPECT_TRUE(test_delegate.ContentBlocked());
  test_delegate.Reset();

  // Check that only the respective content types are affected.
  EXPECT_TRUE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_TRUE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));
  content_settings.OnCookieChanged(
      GURL("http://google.com"), "A=B", options, false);

  // Block a cookie.
  content_settings.OnCookieChanged(
      GURL("http://google.com"), "C=D", options, true);
  EXPECT_TRUE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));

  // Reset blocked content settings.
  content_settings.ClearBlockedContentSettingsExceptForCookies();
  EXPECT_TRUE(test_delegate.SettingsChanged());
  EXPECT_FALSE(test_delegate.ContentBlocked());
  EXPECT_FALSE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  EXPECT_TRUE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_FALSE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));

  content_settings.ClearCookieSpecificContentSettings();
  EXPECT_TRUE(test_delegate.SettingsChanged());
  EXPECT_FALSE(test_delegate.ContentBlocked());
  EXPECT_FALSE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_FALSE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));
}

TEST(TabSpecificContentSettingsTest, AllowedContent) {
  TestContentSettingsDelegate test_delegate;
  TestingProfile profile;
  TabSpecificContentSettings content_settings(&test_delegate, &profile);
  net::CookieOptions options;

  ASSERT_FALSE(
      content_settings.IsContentAccessed(CONTENT_SETTINGS_TYPE_IMAGES));
  ASSERT_FALSE(
      content_settings.IsContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  content_settings.OnCookieChanged(
      GURL("http://google.com"), "A=B", options, false);
  ASSERT_TRUE(
      content_settings.IsContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  content_settings.OnCookieChanged(
      GURL("http://google.com"), "C=D", options, true);
  ASSERT_TRUE(
      content_settings.IsContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_TRUE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}

TEST(TabSpecificContentSettingsTest, EmptyCookieList) {
  TestContentSettingsDelegate test_delegate;
  TestingProfile profile;
  TabSpecificContentSettings content_settings(&test_delegate, &profile);

  ASSERT_FALSE(
      content_settings.IsContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  content_settings.OnCookiesRead(
      GURL("http://google.com"), net::CookieList(), true);
  ASSERT_FALSE(
      content_settings.IsContentAccessed(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}
