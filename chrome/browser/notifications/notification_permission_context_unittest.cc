// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_permission_context.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/time/time.h"
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
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"
#include "url/gurl.h"

namespace {

void DoNothing(ContentSetting content_setting) {}

void StoreContentSetting(ContentSetting* out_content_setting,
                         ContentSetting content_setting) {
  DCHECK(out_content_setting);
  *out_content_setting = content_setting;
}

class TestNotificationPermissionContext : public NotificationPermissionContext {
 public:
  explicit TestNotificationPermissionContext(Profile* profile)
      : NotificationPermissionContext(profile,
                                      CONTENT_SETTINGS_TYPE_NOTIFICATIONS),
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
        base::MakeUnique<base::ScopedMockTimeMessageLoopTaskRunner>();
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

// Web Notification permission requests will completely ignore the embedder
// origin. See https://crbug.com/416894.
TEST_F(NotificationPermissionContextTest, IgnoresEmbedderOrigin) {
  GURL requesting_origin("https://example.com");
  GURL embedding_origin("https://chrome.com");
  GURL different_origin("https://foobar.com");

  NotificationPermissionContext context(profile(),
                                        CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  UpdateContentSetting(&context, requesting_origin, embedding_origin,
                       CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .content_setting);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, different_origin)
                .content_setting);

  context.ResetPermission(requesting_origin, embedding_origin);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .content_setting);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, different_origin)
                .content_setting);
}

// Push messaging permission requests should only succeed for top level origins
// (embedding origin == requesting origin). Retrieving previously granted
// permissions should continue to be possible regardless of being top level.
TEST_F(NotificationPermissionContextTest, PushTopLevelOriginOnly) {
  GURL requesting_origin("https://example.com");
  GURL embedding_origin("https://chrome.com");

  NotificationPermissionContext context(profile(),
                                        CONTENT_SETTINGS_TYPE_PUSH_MESSAGING);

  EXPECT_EQ(CONTENT_SETTING_ASK,
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

// Both Web Notifications and Push Notifications require secure origins.
TEST_F(NotificationPermissionContextTest, SecureOriginRequirement) {
  GURL insecure_origin("http://example.com");

  // Web Notifications
  {
    NotificationPermissionContext web_notification_context(
        profile(), CONTENT_SETTINGS_TYPE_NOTIFICATIONS);

    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              web_notification_context
                  .GetPermissionStatus(nullptr /* render_frame_host */,
                                       insecure_origin, insecure_origin)
                  .content_setting);
  }

  // Push Notifications
  {
    NotificationPermissionContext push_notification_context(
        profile(), CONTENT_SETTINGS_TYPE_PUSH_MESSAGING);

    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              push_notification_context
                  .GetPermissionStatus(nullptr /* render_frame_host */,
                                       insecure_origin, insecure_origin)
                  .content_setting);
  }
}

// Tests auto-denial after a time delay in incognito.
TEST_F(NotificationPermissionContextTest, TestDenyInIncognitoAfterDelay) {
  TestNotificationPermissionContext permission_context(
      profile()->GetOffTheRecordProfile());
  GURL url("https://www.example.com");
  NavigateAndCommit(url);

  const PermissionRequestID id(web_contents()->GetRenderProcessHost()->GetID(),
                               web_contents()->GetMainFrame()->GetRoutingID(),
                               -1);

  base::TestMockTimeTaskRunner* task_runner = SwitchToMockTime();

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.RequestPermission(
      web_contents(), id, url, true /* user_gesture */, base::Bind(&DoNothing));

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

// Tests that navigating cancels incognito permission requests without crashing.
TEST_F(NotificationPermissionContextTest, TestCancelledIncognitoRequest) {
  TestNotificationPermissionContext permission_context(
      profile()->GetOffTheRecordProfile());
  GURL url("https://www.example.com");
  NavigateAndCommit(url);

  const PermissionRequestID id(web_contents()->GetRenderProcessHost()->GetID(),
                               web_contents()->GetMainFrame()->GetRoutingID(),
                               -1);

  base::TestMockTimeTaskRunner* task_runner = SwitchToMockTime();

  PermissionManager* permission_manager =
      PermissionManagerFactory::GetForProfile(
          profile()->GetOffTheRecordProfile());

  // Request and cancel the permission via PermissionManager. That way if
  // https://crbug.com/586944 regresses, then as well as the EXPECT_EQs below
  // failing, PermissionManager::OnPermissionsRequestResponseStatus will crash.
  int request_id = permission_manager->RequestPermission(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, web_contents()->GetMainFrame(),
      url.GetOrigin(), true /* user_gesture */, base::Bind(&DoNothing));

  permission_manager->CancelPermissionRequest(request_id);

  task_runner->FastForwardBy(base::TimeDelta::FromDays(1));

  EXPECT_EQ(0, permission_context.permission_set_count());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context.GetContentSettingFromMap(url, url));
}

// Tests how multiple parallel permission requests get auto-denied in incognito.
TEST_F(NotificationPermissionContextTest, TestParallelDenyInIncognito) {
  TestNotificationPermissionContext permission_context(
      profile()->GetOffTheRecordProfile());
  GURL url("https://www.example.com");
  NavigateAndCommit(url);
  web_contents()->WasShown();

  const PermissionRequestID id0(web_contents()->GetRenderProcessHost()->GetID(),
                                web_contents()->GetMainFrame()->GetRoutingID(),
                                0);
  const PermissionRequestID id1(web_contents()->GetRenderProcessHost()->GetID(),
                                web_contents()->GetMainFrame()->GetRoutingID(),
                                1);

  base::TestMockTimeTaskRunner* task_runner = SwitchToMockTime();

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.RequestPermission(web_contents(), id0, url,
                                       true /* user_gesture */,
                                       base::Bind(&DoNothing));
  permission_context.RequestPermission(web_contents(), id1, url,
                                       true /* user_gesture */,
                                       base::Bind(&DoNothing));

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
