// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_navigation_observer.h"
#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using content::kAboutBlankURL;
using content::WebContents;
using content::PAGE_TRANSITION_TYPED;

class FullscreenControllerBrowserTest: public FullscreenControllerTest {
};

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       PendingMouseLockExitsOnTabSwitch) {
  AddTabAtIndex(0, GURL(kAboutBlankURL), PAGE_TRANSITION_TYPED);
  AddTabAtIndex(0, GURL(kAboutBlankURL), PAGE_TRANSITION_TYPED);
  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();

  // Request mouse lock. Bubble is displayed.
  RequestToLockMouse(true, false);
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());

  // Activate current tab. Mouse lock bubble remains.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());

  // Activate second tab. Mouse lock bubble clears.
  {
    MouseLockNotificationObserver mouse_lock_observer;
    browser()->tab_strip_model()->ActivateTabAt(1, true);
    mouse_lock_observer.Wait();
  }
  ASSERT_FALSE(IsFullscreenBubbleDisplayed());

  // Now, test that closing an unrelated tab does not disturb a request.

  // Request mouse lock. Bubble is displayed.
  RequestToLockMouse(true, false);
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());

  // Close first tab while second active. Mouse lock bubble remains.
  chrome::CloseWebContents(browser(), tab1, false);
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       PendingMouseLockExitsOnTabClose) {
  // Add more tabs.
  AddTabAtIndex(0, GURL(kAboutBlankURL), PAGE_TRANSITION_TYPED);
  AddTabAtIndex(0, GURL(kAboutBlankURL), PAGE_TRANSITION_TYPED);

  // Request mouse lock. Bubble is displayed.
  RequestToLockMouse(true, false);
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());

  // Close tab. Bubble is cleared.
  {
    MouseLockNotificationObserver mouse_lock_observer;
    chrome::CloseTab(browser());
    mouse_lock_observer.Wait();
  }
  ASSERT_FALSE(IsFullscreenBubbleDisplayed());
}
