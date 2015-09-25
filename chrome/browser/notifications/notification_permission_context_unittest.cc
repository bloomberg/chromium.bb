// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_permission_context.h"

#include "base/bind.h"
#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// Web Notification permission requests will completely ignore the embedder
// origin. See https://crbug.com/416894.
TEST(NotificationPermissionContextTest, IgnoresEmbedderOrigin) {
  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;

  GURL requesting_origin("https://example.com");
  GURL embedding_origin("https://chrome.com");
  GURL different_origin("https://foobar.com");

  NotificationPermissionContext context(&profile);
  context.UpdateContentSetting(requesting_origin,
                               embedding_origin,
                               CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
      context.GetPermissionStatus(requesting_origin, embedding_origin));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
      context.GetPermissionStatus(requesting_origin, different_origin));

  context.ResetPermission(requesting_origin, embedding_origin);

  EXPECT_EQ(CONTENT_SETTING_ASK,
      context.GetPermissionStatus(requesting_origin, embedding_origin));

  EXPECT_EQ(CONTENT_SETTING_ASK,
      context.GetPermissionStatus(requesting_origin, different_origin));
}

// Web Notifications do not require a secure origin when requesting permission.
// See https://crbug.com/404095.
TEST(NotificationPermissionContextTest, NoSecureOriginRequirement) {
  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;

  GURL origin("http://example.com");

  NotificationPermissionContext context(&profile);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            context.GetPermissionStatus(origin, origin));

  context.UpdateContentSetting(origin, origin, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            context.GetPermissionStatus(origin, origin));
}
