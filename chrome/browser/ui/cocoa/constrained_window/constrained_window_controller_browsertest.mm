// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#include "chrome/browser/ui/cocoa/run_loop_testing.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"

typedef InProcessBrowserTest ConstrainedWindowControllerTest;

IN_PROC_BROWSER_TEST_F(ConstrainedWindowControllerTest, BasicTest) {
  content::WebContents* tab = browser()->tab_strip_model()->GetWebContentsAt(0);
  NSRect frame = NSMakeRect(0, 0, 100, 50);
  scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:frame]);
  scoped_nsobject<ConstrainedWindowController> window_controller(
    [[ConstrainedWindowController alloc]
        initWithParentWebContents:tab
                     embeddedView:view]);

  // The window should match the view size.
  NSWindow* window = [window_controller window];
  ConstrainedWindowSheetController* sheetController =
      [ConstrainedWindowSheetController controllerForSheet:window];
  EXPECT_TRUE(sheetController);
  EXPECT_TRUE(NSEqualSizes(frame.size, [window frame].size));

  // Test that resizing the view resizes the window.
  NSRect new_frame = NSMakeRect(0, 0, 200, 100);
  [view setFrame:new_frame];
  EXPECT_TRUE(NSEqualSizes(new_frame.size, [window frame].size));

  // Test that closing the window closes the constrained window.
  [window_controller close];
  [sheetController endAnimationForSheet:window];
  chrome::testing::NSRunLoopRunAllPending();
  EXPECT_FALSE([ConstrainedWindowSheetController controllerForSheet:window]);
}
