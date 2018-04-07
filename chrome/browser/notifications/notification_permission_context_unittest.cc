// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_permission_context.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/permissions/permission_status.mojom.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif  // defined(OS_ANDROID)

namespace {

void StoreContentSetting(ContentSetting* out_content_setting,
                         ContentSetting content_setting) {
  DCHECK(out_content_setting);
  *out_content_setting = content_setting;
}

class TestNotificationPermissionContext : public NotificationPermissionContext {
 public:
  explicit TestNotificationPermissionContext(Profile* profile)
      : NotificationPermissionContext(profile),
        permission_set_count_(0),
        last_permission_set_persisted_(false),
        last_permission_set_setting_(CONTENT_SETTING_DEFAULT) {}

  int permission_set_count() const { return permission_set_count_; }
  bool last_permission_set_persisted() const {
    return last_permission_set_persisted_;
  }
  ContentSetting last_permission_set_setting() const {
    return last_permission_set_setting_;
  }

  ContentSetting GetContentSettingFromMap(const GURL& url_a,
                                          const GURL& url_b) {
    return HostContentSettingsMapFactory::GetForProfile(profile())
        ->GetContentSetting(url_a.GetOrigin(), url_b.GetOrigin(),
                            content_settings_type(), std::string());
  }

 private:
  // NotificationPermissionContext:
  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedder_origin,
                           const BrowserPermissionCallback& callback,
                           bool persist,
                           ContentSetting content_setting) override {
    permission_set_count_++;
    last_permission_set_persisted_ = persist;
    last_permission_set_setting_ = content_setting;
    NotificationPermissionContext::NotifyPermissionSet(
        id, requesting_origin, embedder_origin, callback, persist,
        content_setting);
  }

  int permission_set_count_;
  bool last_permission_set_persisted_;
  ContentSetting last_permission_set_setting_;
};

}  // namespace

class NotificationPermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void TearDown() override {
    mock_time_task_runner_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  base::TestMockTimeTaskRunner* SwitchToMockTime() {
    EXPECT_FALSE(mock_time_task_runner_);
    mock_time_task_runner_ =
        std::make_unique<base::ScopedMockTimeMessageLoopTaskRunner>();
    return mock_time_task_runner_->task_runner();
  }

  void UpdateContentSetting(NotificationPermissionContext* context,
                            const GURL& requesting_origin,
                            const GURL& embedding_origin,
                            ContentSetting setting) {
    context->UpdateContentSetting(requesting_origin, embedding_origin, setting);
  }

 private:
  std::unique_ptr<base::ScopedMockTimeMessageLoopTaskRunner>
      mock_time_task_runner_;
};

// Web Notification permission checks will never return ASK for cross-origin
// requests, as permission cannot be requested in that situation.
TEST_F(NotificationPermissionContextTest, CrossOriginPermissionChecks) {
  GURL requesting_origin("https://example.com");
  GURL embedding_origin("https://chrome.com");

  NotificationPermissionContext context(profile());

  // Both same-origin and cross-origin requests for |requesting_origin| should
  // have their default values.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, requesting_origin)
                .content_setting);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .content_setting);

  // Now grant permission for the |requesting_origin|. This should be granted
  // in both contexts.
  UpdateContentSetting(&context, requesting_origin, requesting_origin,
                       CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, requesting_origin)
                .content_setting);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .content_setting);

// Now block permission for |requesting_origin|.

#if defined(OS_ANDROID)
  // On Android O+, permission must be reset before it can be blocked. This is
  // because granting a permission on O+ creates a system-managed notification
  // channel which determines the value of the content setting, so it is not
  // allowed to then toggle the value from ALLOW->BLOCK directly. However,
  // Chrome may reset the permission (which deletes the channel), and *then*
  // grant/block it (creating a new channel).
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_OREO) {
    context.ResetPermission(requesting_origin, requesting_origin);
  }
#endif  // defined(OS_ANDROID)

  UpdateContentSetting(&context, requesting_origin, requesting_origin,
                       CONTENT_SETTING_BLOCK);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, requesting_origin)
                .content_setting);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .content_setting);

  // Resetting the permission should demonstrate the default behaviour again.
  context.ResetPermission(requesting_origin, requesting_origin);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, requesting_origin)
                .content_setting);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .content_setting);
}

// Web Notifications permission requests should only succeed for top level
// origins (embedding origin == requesting origin). Retrieving previously
// granted permissions should continue to be possible regardless of being top
// level.
TEST_F(NotificationPermissionContextTest, WebNotificationsTopLevelOriginOnly) {
  GURL requesting_origin("https://example.com");
  GURL embedding_origin("https://chrome.com");

  NotificationPermissionContext context(profile());

  EXPECT_EQ(CONTENT_SETTING_ASK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, requesting_origin)
                .content_setting);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .content_setting);

  // Requesting permission for different origins should fail.
  PermissionRequestID fake_id(0 /* render_process_id */,
                              0 /* render_frame_id */, 0 /* request_id */);

  ContentSetting result = CONTENT_SETTING_DEFAULT;
  context.DecidePermission(web_contents(), fake_id, requesting_origin,
                           embedding_origin, true /* user_gesture */,
                           base::Bind(&StoreContentSetting, &result));

  ASSERT_EQ(result, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, requesting_origin)
                .content_setting);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .content_setting);

  // Reading previously set permissions should continue to work.
  UpdateContentSetting(&context, requesting_origin, embedding_origin,
                       CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .content_setting);

  context.ResetPermission(requesting_origin, embedding_origin);
}

// Web Notifications require secure origins.
TEST_F(NotificationPermissionContextTest, SecureOriginRequirement) {
  GURL insecure_origin("http://example.com");
  GURL secure_origin("https://chrome.com");

  NotificationPermissionContext web_notification_context(profile());

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            web_notification_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_origin, insecure_origin)
                .content_setting);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            web_notification_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_origin, secure_origin)
                .content_setting);
}

// Tests auto-denial after a time delay in incognito.
TEST_F(NotificationPermissionContextTest, TestDenyInIncognitoAfterDelay) {
  TestNotificationPermissionContext permission_context(
      profile()->GetOffTheRecordProfile());
  GURL url("https://www.example.com");
  NavigateAndCommit(url);

  const PermissionRequestID id(
      web_contents()->GetMainFrame()->GetProcess()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID(), -1);

  base::TestMockTimeTaskRunner* task_runner = SwitchToMockTime();

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.RequestPermission(
      web_contents(), id, url, true /* user_gesture */, base::DoNothing());

  // Should be blocked after 1-2 seconds, but the timer is reset whenever the
  // tab is not visible, so these 500ms never add up to >= 1 second.
  for (int n = 0; n < 10; n++) {
    web_contents()->WasShown();
    task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(500));
    web_contents()->WasHidden();
  }

  EXPECT_EQ(0, permission_context.permission_set_count());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context.GetContentSettingFromMap(url, url));

  // Time elapsed whilst hidden is not counted.
  // n.b. This line also clears out any old scheduled timer tasks. This is
  // important, because otherwise Timer::Reset (triggered by
  // VisibilityTimerTabHelper::WasShown) may choose to re-use an existing
  // scheduled task, and when it fires Timer::RunScheduledTask will call
  // TimeTicks::Now() (which unlike task_runner->NowTicks(), we can't fake),
  // and miscalculate the remaining delay at which to fire the timer.
  task_runner->FastForwardBy(base::TimeDelta::FromDays(1));

  EXPECT_EQ(0, permission_context.permission_set_count());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context.GetContentSettingFromMap(url, url));

  // Should be blocked after 1-2 seconds. So 500ms is not enough.
  web_contents()->WasShown();
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(500));

  EXPECT_EQ(0, permission_context.permission_set_count());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context.GetContentSettingFromMap(url, url));

  // But 5*500ms > 2 seconds, so it should now be blocked.
  for (int n = 0; n < 4; n++)
    task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(500));

  EXPECT_EQ(1, permission_context.permission_set_count());
  EXPECT_TRUE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context.last_permission_set_setting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context.GetContentSettingFromMap(url, url));
}

// Tests how multiple parallel permission requests get auto-denied in incognito.
TEST_F(NotificationPermissionContextTest, TestParallelDenyInIncognito) {
  TestNotificationPermissionContext permission_context(
      profile()->GetOffTheRecordProfile());
  GURL url("https://www.example.com");
  NavigateAndCommit(url);
  web_contents()->WasShown();

  const PermissionRequestID id0(
      web_contents()->GetMainFrame()->GetProcess()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID(), 0);
  const PermissionRequestID id1(
      web_contents()->GetMainFrame()->GetProcess()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID(), 1);

  base::TestMockTimeTaskRunner* task_runner = SwitchToMockTime();

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.RequestPermission(
      web_contents(), id0, url, true /* user_gesture */, base::DoNothing());
  permission_context.RequestPermission(
      web_contents(), id1, url, true /* user_gesture */, base::DoNothing());

  EXPECT_EQ(0, permission_context.permission_set_count());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context.GetContentSettingFromMap(url, url));

  // Fast forward up to 2.5 seconds. Stop as soon as the first permission
  // request is auto-denied.
  for (int n = 0; n < 5; n++) {
    task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(500));
    if (permission_context.permission_set_count())
      break;
  }

  // Only the first permission request receives a response (crbug.com/577336).
  EXPECT_EQ(1, permission_context.permission_set_count());
  EXPECT_TRUE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context.last_permission_set_setting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context.GetContentSettingFromMap(url, url));

  // After another 2.5 seconds, the second permission request should also have
  // received a response.
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(2500));
  EXPECT_EQ(2, permission_context.permission_set_count());
  EXPECT_TRUE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context.last_permission_set_setting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context.GetContentSettingFromMap(url, url));
}
