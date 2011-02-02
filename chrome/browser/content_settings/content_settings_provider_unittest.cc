// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/content_settings/mock_content_settings_provider.h"

namespace content_settings {

TEST(ContentSettingsProviderTest, Mock) {
  MockContentSettingsProvider provider(CONTENT_SETTINGS_TYPE_COOKIES,
                                       CONTENT_SETTING_ALLOW,
                                       false,
                                       true);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_POPUPS));
  EXPECT_FALSE(provider.DefaultSettingIsManaged(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_FALSE(provider.DefaultSettingIsManaged(CONTENT_SETTINGS_TYPE_POPUPS));
  provider.UpdateDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider.ProvideDefaultSetting(CONTENT_SETTINGS_TYPE_COOKIES));
}

}  // namespace content_settings
