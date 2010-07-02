// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_specific_content_settings.h"

#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class TestContentSettingsDelegate
    : public TabSpecificContentSettings::Delegate {
 public:
  TestContentSettingsDelegate() : settings_changed_(false) {}
  virtual ~TestContentSettingsDelegate() {}

  void Reset() { settings_changed_ = false; }

  bool SettingsChanged() { return settings_changed_; }

  // TabSpecificContentSettings::Delegate implementation.
  virtual void OnContentSettingsChange() { settings_changed_ = true; }

 private:
  bool settings_changed_;

  DISALLOW_COPY_AND_ASSIGN(TestContentSettingsDelegate);
};
}  // namespace

TEST(TabSpecificContentSettingsTest, BlockedContent) {
  TestContentSettingsDelegate test_delegate;
  TestingProfile profile;
  TabSpecificContentSettings content_settings(&test_delegate, &profile);

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
  content_settings.OnCookieAccessed(GURL("http://google.com"), "A=B", false);
  EXPECT_FALSE(test_delegate.SettingsChanged());
  test_delegate.Reset();
  content_settings.OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES);
  EXPECT_TRUE(test_delegate.SettingsChanged());
  test_delegate.Reset();
  content_settings.SetPopupsBlocked(true);
  EXPECT_TRUE(test_delegate.SettingsChanged());
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

  // Reset blocked content settings.
  content_settings.ClearBlockedContentSettings();
  EXPECT_TRUE(test_delegate.SettingsChanged());
  EXPECT_FALSE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  EXPECT_FALSE(
      content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_FALSE(content_settings.IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));
}
