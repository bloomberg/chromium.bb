// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

class ContentSettingFramebustBlockBubbleModelTest
    : public InProcessBrowserTest {
 public:
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void WaitForNavigation() {
    content::TestNavigationObserver observer(GetWebContents());
    observer.Wait();
  }
};

// Tests that clicking an item in the list of blocked URLs trigger a navigation
// to that URL.
IN_PROC_BROWSER_TEST_F(ContentSettingFramebustBlockBubbleModelTest,
                       AllowRedirection) {
  const GURL blocked_urls[] = {
      GURL(chrome::kChromeUIHistoryURL), GURL(chrome::kChromeUISettingsURL),
      GURL(chrome::kChromeUIVersionURL),
  };

  // Signal that a blocked redirection happened.
  auto* helper = FramebustBlockTabHelper::FromWebContents(GetWebContents());
  for (const GURL& url : blocked_urls)
    helper->AddBlockedUrl(url);
  EXPECT_TRUE(helper->HasBlockedUrls());

  // Simulate clicking on the second blocked URL.
  ContentSettingFramebustBlockBubbleModel framebust_block_bubble_model(
      browser()->content_setting_bubble_model_delegate(), GetWebContents(),
      browser()->profile());
  framebust_block_bubble_model.OnListItemClicked(/* index = */ 1,
                                                 ui::EF_LEFT_MOUSE_BUTTON);
  WaitForNavigation();

  EXPECT_FALSE(helper->HasBlockedUrls());
  EXPECT_EQ(blocked_urls[1], GetWebContents()->GetLastCommittedURL());
}
