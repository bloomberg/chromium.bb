// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/push_messaging_permission_context.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kOriginA[] = "https://origina.org";
const char kOriginB[] = "https://originb.org";

namespace gcm {

class TestPushMessagingPermissionContext
    : public PushMessagingPermissionContext {
 public:
  explicit TestPushMessagingPermissionContext(Profile* profile)
      : PushMessagingPermissionContext(profile),
        was_persisted_(false),
        permission_granted_(false) {}

  bool was_persisted() const { return was_persisted_; }
  bool was_granted() const { return permission_granted_; }

 private:
  // PushMessagingPermissionContext:
  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedder_origin,
                           const BrowserPermissionCallback& callback,
                           bool persist,
                           ContentSetting content_setting) override {
    was_persisted_ = persist;
    permission_granted_ = content_setting == CONTENT_SETTING_ALLOW;
  }

  bool was_persisted_;
  bool permission_granted_;
};

class PushMessagingPermissionContextTest : public testing::Test {
 public:
  PushMessagingPermissionContextTest() {}

 protected:
  void SetContentSetting(Profile* profile,
                         ContentSettingsType setting,
                         ContentSetting value) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(kOriginA);
    HostContentSettingsMap* host_content_settings_map =
        profile->GetHostContentSettingsMap();
    host_content_settings_map->SetContentSetting(pattern, pattern, setting,
                                                 std::string(), value);
  }

  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(PushMessagingPermissionContextTest, HasPermissionPrompt) {
  TestingProfile profile;
  PushMessagingPermissionContext context(&profile);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));

  // Just granting notifications should still prompt
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                    CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));

  // Just granting push should still prompt
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                    CONTENT_SETTING_ASK);
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));
}

TEST_F(PushMessagingPermissionContextTest, HasPermissionDenySettingsMismatch) {
  TestingProfile profile;
  PushMessagingPermissionContext context(&profile);
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                    CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                    CONTENT_SETTING_ASK);
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                    CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));

  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                    CONTENT_SETTING_ASK);
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));
}

TEST_F(PushMessagingPermissionContextTest, HasPermissionDenyDifferentOrigins) {
  TestingProfile profile;
  PushMessagingPermissionContext context(&profile);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kOriginB), GURL(kOriginA)));
}

TEST_F(PushMessagingPermissionContextTest, HasPermissionAccept) {
  TestingProfile profile;
  PushMessagingPermissionContext context(&profile);

  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                    CONTENT_SETTING_ALLOW);
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));
}

TEST_F(PushMessagingPermissionContextTest, DecidePushPermission) {
  TestingProfile profile;
  TestPushMessagingPermissionContext context(&profile);
  PermissionRequestID request_id(-1, -1, -1, GURL(kOriginA));
  BrowserPermissionCallback callback;

  context.DecidePushPermission(request_id, GURL(kOriginA), GURL(kOriginA),
                               callback, CONTENT_SETTING_DEFAULT);
  EXPECT_FALSE(context.was_persisted());
  EXPECT_FALSE(context.was_granted());

  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_ALLOW);
  context.DecidePushPermission(request_id, GURL(kOriginA), GURL(kOriginA),
                               callback, CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(context.was_persisted());
  EXPECT_TRUE(context.was_granted());

  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_BLOCK);
  context.DecidePushPermission(request_id, GURL(kOriginA), GURL(kOriginA),
                               callback, CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(context.was_persisted());
  EXPECT_FALSE(context.was_granted());

  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_ASK);
  context.DecidePushPermission(request_id, GURL(kOriginA), GURL(kOriginA),
                               callback, CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(context.was_persisted());
  EXPECT_TRUE(context.was_granted());
}

}  // namespace gcm
