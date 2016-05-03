// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_permission_context.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kOriginA[] = "https://origina.org";
const char kOriginB[] = "https://originb.org";
const char kInsecureOrigin[] = "http://insecureorigin.org";

void DoNothing(ContentSetting content_setting) {}

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
    PushMessagingPermissionContext::NotifyPermissionSet(
        id, requesting_origin, embedder_origin, callback, persist,
        content_setting);
  }

  bool was_persisted_;
  bool permission_granted_;
};

class PushMessagingPermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PushMessagingPermissionContextTest() {}

 protected:
  void SetContentSetting(Profile* profile,
                         ContentSettingsType setting,
                         ContentSetting value) {
    // These urls must match those in
    // PermissionContextBase::UpdateContentSetting, since the tests below use
    // this method to overwrite urls set as a result of
    // PushMessagingPermissionContext::NotifyPermissionSet.
    GURL url_a = GURL(kOriginA);
    GURL insecure_url = GURL(kInsecureOrigin);
    HostContentSettingsMap* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile);

    host_content_settings_map->SetContentSettingDefaultScope(
        url_a, url_a, setting, std::string(), value);
    host_content_settings_map->SetContentSettingDefaultScope(
        insecure_url, insecure_url, setting, std::string(), value);
  }
};

}  // namespace

TEST_F(PushMessagingPermissionContextTest, HasPermissionPrompt) {
  TestingProfile profile;
  PushMessagingPermissionContext context(&profile);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));

  // Just granting push messaging permission should still prompt.
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));

  // Just granting notifications should allow if push messaging is not blocked.
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                    CONTENT_SETTING_ASK);
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));

  // Granting allow notifications but explicitly denying push messaging should
  // block.
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                    CONTENT_SETTING_ALLOW);
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_BLOCK);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
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
  PermissionRequestID request_id(-1, -1, -1);
  BrowserPermissionCallback callback = base::Bind(DoNothing);

  context.DecidePushPermission(request_id, GURL(kOriginA), GURL(kOriginA),
                               callback,
                               blink::mojom::PermissionStatus::DENIED);
  EXPECT_FALSE(context.was_persisted());
  EXPECT_FALSE(context.was_granted());

  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_ALLOW);
  context.DecidePushPermission(request_id, GURL(kOriginA), GURL(kOriginA),
                               callback,
                               blink::mojom::PermissionStatus::GRANTED);
  EXPECT_TRUE(context.was_persisted());
  EXPECT_TRUE(context.was_granted());

  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_BLOCK);
  context.DecidePushPermission(request_id, GURL(kOriginA), GURL(kOriginA),
                               callback,
                               blink::mojom::PermissionStatus::GRANTED);
  EXPECT_TRUE(context.was_persisted());
  EXPECT_FALSE(context.was_granted());

  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_ASK);
  context.DecidePushPermission(request_id, GURL(kOriginA), GURL(kOriginA),
                               callback,
                               blink::mojom::PermissionStatus::GRANTED);
  EXPECT_TRUE(context.was_persisted());
  EXPECT_TRUE(context.was_granted());
}

TEST_F(PushMessagingPermissionContextTest, DecidePermission) {
  TestingProfile profile;
  TestPushMessagingPermissionContext context(&profile);
  PermissionRequestID request_id(-1, -1, -1);
  BrowserPermissionCallback callback = base::Bind(DoNothing);

  // Requesting and embedding origin are different.
  context.DecidePermission(NULL, request_id, GURL(kOriginA), GURL(kOriginB),
                           callback);
  EXPECT_FALSE(context.was_persisted());
  EXPECT_FALSE(context.was_granted());

  // Insecure origin
  NavigateAndCommit(GURL(kInsecureOrigin));
  context.RequestPermission(web_contents(), request_id, GURL(kInsecureOrigin),
                            callback);
  EXPECT_FALSE(context.was_persisted());
  EXPECT_FALSE(context.was_granted());
}

TEST_F(PushMessagingPermissionContextTest, RequestPermission) {
  TestingProfile profile;
  TestPushMessagingPermissionContext context(&profile);
  PermissionRequestID request_id(-1, -1, -1);
  BrowserPermissionCallback callback = base::Bind(DoNothing);

  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                    CONTENT_SETTING_ALLOW);

  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      HostContentSettingsMapFactory::GetForProfile(&profile)->GetContentSetting(
          GURL(kOriginA), GURL(kOriginA), CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
          std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));

  // If a website already has notifications permission, push permission is
  // silently assumed to be granted.
  NavigateAndCommit(GURL(kOriginA));
  context.RequestPermission(web_contents(), request_id, GURL(kOriginA),
                            callback);

  // Although the permission check goes through, the push message permission
  // isn't actually updated.
  EXPECT_TRUE(context.was_granted());
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      HostContentSettingsMapFactory::GetForProfile(&profile)->GetContentSetting(
          GURL(kOriginA), GURL(kOriginA), CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
          std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));
}

TEST_F(PushMessagingPermissionContextTest, RequestAfterRevokingNotifications) {
  TestingProfile profile;
  TestPushMessagingPermissionContext context(&profile);
  PermissionRequestID request_id(-1, -1, -1);
  BrowserPermissionCallback callback = base::Bind(DoNothing);

  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                    CONTENT_SETTING_ALLOW);

  NavigateAndCommit(GURL(kOriginA));
  context.RequestPermission(web_contents(), request_id, GURL(kOriginA),
                            callback);
  EXPECT_TRUE(context.was_granted());

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));

  // Revoke notifications permission. This should revoke push, and prevent
  // future requests for push from succeeding.
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                    CONTENT_SETTING_BLOCK);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));

  context.RequestPermission(web_contents(), request_id, GURL(kOriginA),
                            callback);
  EXPECT_FALSE(context.was_persisted());
  EXPECT_FALSE(context.was_granted());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kOriginA), GURL(kOriginA)));
}

TEST_F(PushMessagingPermissionContextTest, GetPermissionStatusInsecureOrigin) {
  TestingProfile profile;
  TestPushMessagingPermissionContext context(&profile);

  // The status should be blocked for an insecure origin, regardless of the
  // content setting value.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kInsecureOrigin),
                                        GURL(kInsecureOrigin)));

  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_ALLOW);
  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                    CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kInsecureOrigin),
                                        GURL(kInsecureOrigin)));

  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kInsecureOrigin),
                                        GURL(kInsecureOrigin)));

  SetContentSetting(&profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING,
                    CONTENT_SETTING_ASK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context.GetPermissionStatus(GURL(kInsecureOrigin),
                                        GURL(kInsecureOrigin)));
}
