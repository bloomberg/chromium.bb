// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity.h"
#include "athena/resource_manager/public/resource_manager.h"
#include "athena/test/base/test_util.h"
#include "athena/test/chrome/athena_chrome_browser_test.h"
#include "athena/wm/public/window_list_provider.h"
#include "athena/wm/public/window_manager.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace athena {

namespace {
// The test URL to navigate to.
const char kTestUrl[] = "chrome:about";
}

typedef AthenaChromeBrowserTest WebActivityBrowserTest;

// A simple test to create web content.
IN_PROC_BROWSER_TEST_F(WebActivityBrowserTest, SimpleCreate) {
  const GURL target_url(kTestUrl);
  // Create an activity, wait until it is loaded and check that it was created.
  Activity* activity = test_util::CreateTestWebActivity(
      GetBrowserContext(), base::UTF8ToUTF16("App"), target_url);
  ASSERT_TRUE(activity);
  EXPECT_NE(Activity::ACTIVITY_UNLOADED, activity->GetCurrentState());
  // The activity manager should take care of destroying the activity upon
  // shutdown.
}

// A test to load, unload and reload content, verifying that it is getting
// loaded / unloaded properly.
IN_PROC_BROWSER_TEST_F(WebActivityBrowserTest, LoadUnloadReload) {
  const GURL target_url(kTestUrl);
  const aura::Window::Windows& list =
      WindowManager::Get()->GetWindowListProvider()->GetWindowList();

  // Create an activity (and wait until it is loaded).
  // The size of its overview image should be empty since it is visible.
  Activity* activity2 = test_util::CreateTestWebActivity(
      GetBrowserContext(), base::UTF8ToUTF16("App2"), target_url);
  EXPECT_TRUE(activity2);
  EXPECT_EQ(list[0], activity2->GetWindow());
  EXPECT_NE(Activity::ACTIVITY_UNLOADED, activity2->GetCurrentState());
  Activity* activity1 = test_util::CreateTestWebActivity(
      GetBrowserContext(), base::UTF8ToUTF16("App1"), target_url);
  EXPECT_TRUE(activity1);

  // |activity2| should now be the second activity. Both activities should have
  // an active render view and the same, valid SiteURL.
  EXPECT_EQ(list[0], activity2->GetWindow());
  EXPECT_EQ(list[1], activity1->GetWindow());
  GURL url = activity1->GetWebContents()->GetSiteInstance()->GetSiteURL();
  EXPECT_FALSE(url.is_empty());
  EXPECT_EQ(url, activity2->GetWebContents()->GetSiteInstance()->GetSiteURL());
  EXPECT_TRUE(
      activity1->GetWebContents()->GetRenderViewHost()->IsRenderViewLive());
  EXPECT_TRUE(
      activity2->GetWebContents()->GetRenderViewHost()->IsRenderViewLive());

  // Unload the second activity. The window should still be there.
  activity2->SetCurrentState(Activity::ACTIVITY_UNLOADED);
  test_util::WaitUntilIdle();
  EXPECT_EQ(list[0], activity2->GetWindow());
  EXPECT_EQ(Activity::ACTIVITY_UNLOADED, activity2->GetCurrentState());

  // There should be no change to the first activity, but the second one should
  // neither have a SiteURL, nor a RenderView.
  EXPECT_EQ(url, activity1->GetWebContents()->GetSiteInstance()->GetSiteURL());
  EXPECT_TRUE(
      activity2->GetWebContents()->GetSiteInstance()->GetSiteURL().is_empty());
  EXPECT_TRUE(
      activity1->GetWebContents()->GetRenderViewHost()->IsRenderViewLive());
  EXPECT_FALSE(
      activity2->GetWebContents()->GetRenderViewHost()->IsRenderViewLive());

  // Load it again.
  activity2->SetCurrentState(Activity::ACTIVITY_INVISIBLE);
  EXPECT_EQ(list[0], activity2->GetWindow());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, activity2->GetCurrentState());
  EXPECT_EQ(url, activity1->GetWebContents()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(url, activity2->GetWebContents()->GetSiteInstance()->GetSiteURL());
  EXPECT_TRUE(
      activity1->GetWebContents()->GetRenderViewHost()->IsRenderViewLive());
  EXPECT_TRUE(
      activity2->GetWebContents()->GetRenderViewHost()->IsRenderViewLive());
}


}  // namespace athena
