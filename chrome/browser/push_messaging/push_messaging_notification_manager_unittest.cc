// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_notification_manager.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class PushMessagingNotificationManagerTest
    : public ChromeRenderViewHostTestHarness {};

TEST_F(PushMessagingNotificationManagerTest, IsTabVisible) {
  PushMessagingNotificationManager manager(profile());
  GURL origin("https://google.com/");
  NavigateAndCommit(origin);

  EXPECT_FALSE(manager.IsTabVisible(profile(), nullptr, origin));
  EXPECT_FALSE(manager.IsTabVisible(profile(), web_contents(),
                                    GURL("https://chrome.com/")));
  EXPECT_TRUE(manager.IsTabVisible(profile(), web_contents(), origin));

  content::RenderViewHostTester::For(rvh())->SimulateWasHidden();
  EXPECT_FALSE(manager.IsTabVisible(profile(), web_contents(), origin));

  content::RenderViewHostTester::For(rvh())->SimulateWasShown();
  EXPECT_TRUE(manager.IsTabVisible(profile(), web_contents(), origin));
}
