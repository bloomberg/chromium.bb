// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/content_settings/content_settings_mock_provider.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "googleurl/src/gurl.h"

namespace content_settings {

TEST(ContentSettingsProviderTest, Mock) {
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]youtube.com");
  GURL url("http://www.youtube.com");

  MockProvider mock_provider(
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS,
      "java_plugin",
      CONTENT_SETTING_BLOCK,
      false,
      false);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&mock_provider, url, url,
                              CONTENT_SETTINGS_TYPE_PLUGINS, "java_plugin",
                              false));
  scoped_ptr<Value> value_ptr(
            GetContentSettingValue(&mock_provider, url, url,
                                   CONTENT_SETTINGS_TYPE_PLUGINS,
                                   "java_plugin", false));
  int int_value = -1;
  value_ptr->GetAsInteger(&int_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, IntToContentSetting(int_value));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSetting(&mock_provider, url, url,
                              CONTENT_SETTINGS_TYPE_PLUGINS, "flash_plugin",
                              false));
  EXPECT_EQ(NULL,
            GetContentSettingValue(&mock_provider, url, url,
                                   CONTENT_SETTINGS_TYPE_PLUGINS,
                                   "flash_plugin", false));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSetting(&mock_provider, url, url,
                              CONTENT_SETTINGS_TYPE_GEOLOCATION, "", false));
  EXPECT_EQ(NULL,
            GetContentSettingValue(&mock_provider, url, url,
                                   CONTENT_SETTINGS_TYPE_GEOLOCATION, "",
                                   false));

  bool owned = mock_provider.SetWebsiteSetting(
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS,
      "java_plugin",
      Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));
  EXPECT_TRUE(owned);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(&mock_provider, url, url,
                              CONTENT_SETTINGS_TYPE_PLUGINS, "java_plugin",
                              false));

  mock_provider.set_read_only(true);
  scoped_ptr<base::Value> value(
      Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  owned = mock_provider.SetWebsiteSetting(
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS,
      "java_plugin",
      value.get());
  EXPECT_FALSE(owned);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(&mock_provider, url, url,
                              CONTENT_SETTINGS_TYPE_PLUGINS, "java_plugin",
                              false));

  EXPECT_TRUE(mock_provider.read_only());

  mock_provider.set_read_only(false);
  owned = mock_provider.SetWebsiteSetting(
      pattern,
      pattern,
      CONTENT_SETTINGS_TYPE_PLUGINS,
      "java_plugin",
      Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  EXPECT_TRUE(owned);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(&mock_provider, url, url,
                              CONTENT_SETTINGS_TYPE_PLUGINS, "java_plugin",
                              false));
}

}  // namespace content_settings
