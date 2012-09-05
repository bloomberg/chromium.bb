// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac2.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"

class ConstrainedWindowMacTest : public InProcessBrowserTest {
 public:
  ConstrainedWindowMacTest() : InProcessBrowserTest() {
    sheet_.reset([[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 30, 30)
                  styleMask:NSTitledWindowMask
                    backing:NSBackingStoreBuffered
                      defer:NO]);
    [sheet_ setReleasedWhenClosed:NO];
  }

 protected:
  scoped_nsobject<NSWindow> sheet_;
};

// Test that a sheet added to a non-active tab is not shown until the
// tab is activated.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowMacTest, ShowInNonActiveTab) {
  AddTabAtIndex(1, GURL("about:blank"), content::PAGE_TRANSITION_LINK);
  TabContents* tab0 = browser()->tab_strip_model()->GetTabContentsAt(0);
  TabContents* tab1 = browser()->tab_strip_model()->GetTabContentsAt(1);
  EXPECT_EQ(tab1, browser()->tab_strip_model()->GetActiveTabContents());

  // Show dialog in non active tab.
  // Dialog will delete it self when closed.
  ConstrainedWindowMac2* dialog = new ConstrainedWindowMac2(tab0, sheet_);
  EXPECT_EQ(0.0, [sheet_ alphaValue]);

  // Switch to non active tab.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_EQ(1.0, [sheet_ alphaValue]);

  dialog->CloseConstrainedWindow();
}

// Test that adding a sheet disables tab dragging.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowMacTest, TabDragging) {
  AddTabAtIndex(1, GURL("about:blank"), content::PAGE_TRANSITION_LINK);
  TabContents* tab1 = browser()->tab_strip_model()->GetTabContentsAt(1);
  BrowserWindowController* controller = [BrowserWindowController
      browserWindowControllerForView:tab1->web_contents()->GetNativeView()];
  NSView* tab_view0 = [[controller tabStripController] viewAtIndex:0];
  NSView* tab_view1 = [[controller tabStripController] viewAtIndex:1];

  // Dialog will delete it self when closed.
  ConstrainedWindowMac2* dialog = new ConstrainedWindowMac2(tab1, sheet_);

  // Verify that the dialog disables dragging.
  ASSERT_TRUE(controller);
  EXPECT_TRUE([controller isTabDraggable:tab_view0]);
  EXPECT_FALSE([controller isTabDraggable:tab_view1]);

  dialog->CloseConstrainedWindow();
}
