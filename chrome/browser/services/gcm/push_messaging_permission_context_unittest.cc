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

  // PushMessagingPermissionContext:
  void DecidePermission(content::WebContents* web_contents,
                        const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        const GURL& embedder_origin,
                        bool user_gesture,
                        const BrowserPermissionCallback& callback) override {
    PushMessagingPermissionContext::DecidePermission(
        web_contents, id, requesting_origin, embedder_origin, user_gesture,
        callback);
  }

 private:
  // PushMessagingPermissionContext:
  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedder_origin,
                           const BrowserPermissionCallback& callback,
                           bool persist,
                           bool allowed) override {
    was_persisted_ = persist;
    permission_granted_ = allowed;
  }

  bool was_persisted_;
  bool permission_granted_;
};

class PushMessagingPermissionContextTest : public testing::Test {
 public:
  PushMessagingPermissionContextTest() {}

  void SetUp() override {
    HostContentSettingsMap* host_content_settings_map =
        profile_.GetHostContentSettingsMap();
    host_content_settings_map->SetDefaultContentSetting(
        CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ASK);
    host_content_settings_map->SetDefaultContentSetting(
        CONTENT_SETTINGS_TYPE_PUSH_MESSAGING, CONTENT_SETTING_ASK);
  }

 protected:
  void SetContentSetting(ContentSettingsType setting, ContentSetting value) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(kOriginA);
    HostContentSettingsMap* host_content_settings_map =
        profile_.GetHostContentSettingsMap();
    host_content_settings_map->SetContentSetting(pattern, pattern, setting,
                                                 std::string(), value);
  }

  TestingProfile profile_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(PushMessagingPermissionContextTest, HasPermissionPrompt) {
  PushMessagingPermissionContext context(&profile_);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));

  // Just granting notifications should still prompt
  SetContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));

  // Just granting push should still prompt
  SetContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ASK);
  SetContentSetting(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));
}

TEST_F(PushMessagingPermissionContextTest, HasPermissionDenySettingsMismatch) {
  PushMessagingPermissionContext context(&profile_);
  SetContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));
  SetContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ASK);
  SetContentSetting(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));
  SetContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));

  SetContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ASK);
  SetContentSetting(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));
}

TEST_F(PushMessagingPermissionContextTest, HasPermissionDenyDifferentOrigins) {
  PushMessagingPermissionContext context(&profile_);
  SetContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  SetContentSetting(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING, CONTENT_SETTING_ASK);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kOriginB), GURL(kOriginA)));
}

TEST_F(PushMessagingPermissionContextTest, HasPermissionAccept) {
  PushMessagingPermissionContext context(&profile_);
  SetContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  SetContentSetting(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));
}

TEST_F(PushMessagingPermissionContextTest, DecidePermission) {
  TestPushMessagingPermissionContext context(&profile_);
  PermissionRequestID request_id(-1, -1, -1, GURL(kOriginA));
  BrowserPermissionCallback callback;

  context.DecidePermission(NULL, request_id, GURL(kOriginA), GURL(kOriginA),
                           true, callback);
  EXPECT_FALSE(context.was_persisted());
  EXPECT_FALSE(context.was_granted());

  SetContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_BLOCK);
  context.DecidePermission(NULL, request_id, GURL(kOriginA), GURL(kOriginA),
                           true, callback);
  EXPECT_FALSE(context.was_persisted());
  EXPECT_FALSE(context.was_granted());
  SetContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  SetContentSetting(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_BLOCK);
  context.DecidePermission(NULL, request_id, GURL(kOriginA), GURL(kOriginA),
                           true, callback);
  EXPECT_FALSE(context.was_persisted());
  EXPECT_FALSE(context.was_granted());
  SetContentSetting(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING, CONTENT_SETTING_ASK);
  context.DecidePermission(NULL, request_id, GURL(kOriginA), GURL(kOriginA),
                           true, callback);
  EXPECT_TRUE(context.was_persisted());
  EXPECT_TRUE(context.was_granted());
  context.DecidePermission(NULL, request_id, GURL(kOriginB), GURL(kOriginA),
                           true, callback);
  EXPECT_FALSE(context.was_persisted());
  EXPECT_FALSE(context.was_granted());
}

}  // namespace gcm
