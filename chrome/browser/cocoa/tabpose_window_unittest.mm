// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tabpose_window.h"

#import "chrome/browser/browser_window.h"
#import "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabposeWindowTest : public CocoaTest {
 public:
  BrowserTestHelper browser_helper_;
};

// Check that this doesn't leak.
TEST_F(TabposeWindowTest, TestShow) {
  Browser* browser = browser_helper_.browser();
  BrowserWindow* browser_window = browser_helper_.CreateBrowserWindow();
  NSWindow* parent = browser_window->GetNativeHandle();

  [parent orderFront:nil];
  EXPECT_TRUE([parent isVisible]);

  // Add a few tabs to the tab strip model.
  TabStripModel* model = browser->tabstrip_model();
  SiteInstance* instance =
      SiteInstance::CreateSiteInstance(browser_helper_.profile());
  for (int i = 0; i < 3; ++i) {
    TabContents* tab_contents =
        new TabContents(browser_helper_.profile(), instance, MSG_ROUTING_NONE,
                        NULL);
    model->AppendTabContents(tab_contents, /*foreground=*/true);
  }

  base::ScopedNSAutoreleasePool pool;
  TabposeWindow* window =
      [TabposeWindow openTabposeFor:parent
                               rect:NSMakeRect(10, 20, 250, 160)
                              slomo:NO
                      tabStripModel:model];

  // Should release the window.
  [window mouseDown:nil];

  browser_helper_.CloseBrowserWindow();
}
