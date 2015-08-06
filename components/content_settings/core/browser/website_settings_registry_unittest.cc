// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/values.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {

class WebsiteSettingsRegistryTest : public testing::Test {
 protected:
  const WebsiteSettingsRegistry* registry() { return &registry_; }

 private:
  WebsiteSettingsRegistry registry_;
};

TEST_F(WebsiteSettingsRegistryTest, Get) {
  // CONTENT_SETTINGS_TYPE_COOKIES should be registered.
  const WebsiteSettingsInfo* info =
      registry()->Get(CONTENT_SETTINGS_TYPE_COOKIES);
  ASSERT_TRUE(info);
  EXPECT_EQ(CONTENT_SETTINGS_TYPE_COOKIES, info->type());
  EXPECT_EQ("cookies", info->name());
}

TEST_F(WebsiteSettingsRegistryTest, GetByName) {
  // Random string shouldn't be registered.
  EXPECT_FALSE(registry()->GetByName("abc"));

  // "cookies" should be registered.
  const WebsiteSettingsInfo* info = registry()->GetByName("cookies");
  ASSERT_TRUE(info);
  EXPECT_EQ(CONTENT_SETTINGS_TYPE_COOKIES, info->type());
  EXPECT_EQ("cookies", info->name());
  EXPECT_EQ(registry()->Get(CONTENT_SETTINGS_TYPE_COOKIES), info);
}

TEST_F(WebsiteSettingsRegistryTest, Properties) {
  const WebsiteSettingsInfo* info =
      registry()->Get(CONTENT_SETTINGS_TYPE_COOKIES);
  ASSERT_TRUE(info);
  EXPECT_EQ("profile.content_settings.exceptions.cookies", info->pref_name());
  EXPECT_EQ("profile.default_content_setting_values.cookies",
            info->default_value_pref_name());
  int setting;
  ASSERT_TRUE(info->initial_default_value()->GetAsInteger(&setting));
  EXPECT_EQ(CONTENT_SETTING_ALLOW, setting);

  info = registry()->Get(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  ASSERT_TRUE(info);
  EXPECT_EQ("profile.content_settings.exceptions.media_stream_camera",
            info->pref_name());
  EXPECT_EQ("profile.default_content_setting_values.media_stream_camera",
            info->default_value_pref_name());
  ASSERT_TRUE(info->initial_default_value()->GetAsInteger(&setting));
  EXPECT_EQ(CONTENT_SETTING_ASK, setting);
}

}  // namespace content_settings
