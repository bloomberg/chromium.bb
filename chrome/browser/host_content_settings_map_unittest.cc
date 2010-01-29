// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/host_content_settings_map.h"

#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace {

class HostContentSettingsMapTest : public testing::Test {
 public:
  HostContentSettingsMapTest()
    : ui_thread_(ChromeThread::UI, &message_loop_) {}

 protected:
  MessageLoop message_loop_;
  ChromeThread ui_thread_;
};

TEST_F(HostContentSettingsMapTest, DefaultValues) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      profile.GetHostContentSettingsMap();

  // Check setting of default permissions.
  ContentPermissions perm;
  ASSERT_TRUE(host_content_settings_map->SetDefaultContentPermission(
              CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_PERMISSION_TYPE_BLOCK));
  ASSERT_TRUE(host_content_settings_map->SetDefaultContentPermission(
              CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_PERMISSION_TYPE_ASK));
  ASSERT_FALSE(host_content_settings_map->SetDefaultContentPermission(
               CONTENT_SETTINGS_TYPE_JAVASCRIPT,
               CONTENT_PERMISSION_TYPE_DEFAULT));
  ASSERT_TRUE(host_content_settings_map->SetDefaultContentPermission(
              CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_PERMISSION_TYPE_ALLOW));

  // Check per host permissions returned.
  perm.permissions[CONTENT_SETTINGS_TYPE_IMAGES] =
    CONTENT_PERMISSION_TYPE_DEFAULT;
  perm.permissions[CONTENT_SETTINGS_TYPE_PLUGINS] =
    CONTENT_PERMISSION_TYPE_ALLOW;
  perm.permissions[CONTENT_SETTINGS_TYPE_JAVASCRIPT] =
    CONTENT_PERMISSION_TYPE_ALLOW;
  host_content_settings_map->SetPerHostContentSettings("example.com", perm);
  perm = host_content_settings_map->GetPerHostContentSettings("example.com");
  EXPECT_EQ(CONTENT_PERMISSION_TYPE_BLOCK,
            perm.permissions[CONTENT_SETTINGS_TYPE_IMAGES]);
  EXPECT_EQ(CONTENT_PERMISSION_TYPE_ALLOW,
            perm.permissions[CONTENT_SETTINGS_TYPE_PLUGINS]);
  EXPECT_EQ(CONTENT_PERMISSION_TYPE_ALLOW,
            perm.permissions[CONTENT_SETTINGS_TYPE_JAVASCRIPT]);
  perm = host_content_settings_map->GetPerHostContentSettings("example.org");
  EXPECT_EQ(CONTENT_PERMISSION_TYPE_BLOCK,
            perm.permissions[CONTENT_SETTINGS_TYPE_IMAGES]);
  EXPECT_EQ(CONTENT_PERMISSION_TYPE_ASK,
            perm.permissions[CONTENT_SETTINGS_TYPE_PLUGINS]);
  EXPECT_EQ(CONTENT_PERMISSION_TYPE_ALLOW,
            perm.permissions[CONTENT_SETTINGS_TYPE_JAVASCRIPT]);

  // Check returning settings for a given resource.
  HostContentSettingsMap::HostContentPermissions permissions;
  permissions = host_content_settings_map->GetAllPerHostContentPermissions(
                    CONTENT_SETTINGS_TYPE_IMAGES);
  EXPECT_EQ(0U, permissions.size());
  permissions = host_content_settings_map->GetAllPerHostContentPermissions(
                    CONTENT_SETTINGS_TYPE_PLUGINS);
  EXPECT_EQ(1U, permissions.size());
  EXPECT_EQ("example.com", permissions.begin()->first);
  EXPECT_EQ(CONTENT_PERMISSION_TYPE_ALLOW, permissions.begin()->second);
}

TEST_F(HostContentSettingsMapTest, ContentPermissions) {
  ContentPermissions perm;
  perm.permissions[CONTENT_SETTINGS_TYPE_IMAGES] =
    CONTENT_PERMISSION_TYPE_BLOCK;
  perm.permissions[CONTENT_SETTINGS_TYPE_PLUGINS] =
    CONTENT_PERMISSION_TYPE_ASK;
  perm.permissions[CONTENT_SETTINGS_TYPE_JAVASCRIPT] =
    CONTENT_PERMISSION_TYPE_ALLOW;

  int serialized = ContentPermissions::ToInteger(perm);
  ContentPermissions result = ContentPermissions::FromInteger(serialized);

  EXPECT_EQ(perm.permissions[CONTENT_SETTINGS_TYPE_IMAGES],
            result.permissions[CONTENT_SETTINGS_TYPE_IMAGES]);
  EXPECT_EQ(perm.permissions[CONTENT_SETTINGS_TYPE_PLUGINS],
            result.permissions[CONTENT_SETTINGS_TYPE_PLUGINS]);
  EXPECT_EQ(perm.permissions[CONTENT_SETTINGS_TYPE_JAVASCRIPT],
            result.permissions[CONTENT_SETTINGS_TYPE_JAVASCRIPT]);
}

}  // namespace
