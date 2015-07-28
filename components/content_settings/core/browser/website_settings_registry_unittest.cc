// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
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

}  // namespace content_settings
