// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/dev_tools_controller.h"

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"

class DevToolsControllerTest : public InProcessBrowserTest {
 public:
  DevToolsControllerTest() : InProcessBrowserTest() {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    DevToolsWindow::ToggleDevToolsWindow(browser(),
                                         DEVTOOLS_TOGGLE_ACTION_SHOW);
  }

  DevToolsController* controller() {
    NSWindow* window = browser()->window()->GetNativeWindow();
    BrowserWindowController* window_controller =
        [BrowserWindowController browserWindowControllerForWindow:window];
    return [window_controller devToolsController];
  }

  void SetDockSide(DevToolsDockSide side) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    DevToolsWindow* dev_tools =
        DevToolsWindow::GetDockedInstanceForInspectedTab(web_contents);
    dev_tools->SetDockSide(dev_tools->SideToString(side));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsControllerTest);
};

// Verify that in horizontal mode the splitter is not allowed to go past the
// bookmark bar.
IN_PROC_BROWSER_TEST_F(DevToolsControllerTest, ConstrainSplitter) {
  [controller() setTopContentOffset:0];
  EXPECT_EQ(0, [controller() splitView:[controller() splitView]
                constrainSplitPosition:0
                           ofSubviewAt:0]);

  CGFloat offset = 50;
  [controller() setTopContentOffset:offset];
  EXPECT_EQ(offset, [controller() splitView:[controller() splitView]
                     constrainSplitPosition:0
                                ofSubviewAt:0]);

  // Should not be constrained in vertical mode.
  [[controller() splitView] setVertical:YES];
  EXPECT_EQ(0, [controller() splitView:[controller() splitView]
                constrainSplitPosition:0
                           ofSubviewAt:0]);
}

// When docked to the right the dev tools view should be shrunk so that it
// doesn't overlap the bookmark bar.
IN_PROC_BROWSER_TEST_F(DevToolsControllerTest, ViewSize) {
  EXPECT_EQ(2u, [[[controller() splitView] subviews] count]);
  NSView* container_view = [[[controller() splitView] subviews] lastObject];
  EXPECT_EQ(1u, [[container_view subviews] count]);
  NSView* dev_tools_view = [[container_view subviews] lastObject];
  CGFloat width = NSWidth([[controller() splitView] bounds]);
  CGFloat height = NSHeight([[controller() splitView] bounds]);
  CGFloat offset = [controller() topContentOffset];

  SetDockSide(DEVTOOLS_DOCK_SIDE_BOTTOM);
  EXPECT_EQ(width, NSWidth([dev_tools_view bounds]));

  SetDockSide(DEVTOOLS_DOCK_SIDE_RIGHT);
  EXPECT_EQ(height - offset, NSHeight([dev_tools_view bounds]));

  CGFloat new_offset = 50;
  [controller() setTopContentOffset:new_offset];

  SetDockSide(DEVTOOLS_DOCK_SIDE_BOTTOM);
  EXPECT_EQ(width, NSWidth([dev_tools_view bounds]));

  SetDockSide(DEVTOOLS_DOCK_SIDE_RIGHT);
  EXPECT_EQ(height - new_offset, NSHeight([dev_tools_view bounds]));
}

// Verify that the dev tool's web view is layed out correctly when docked to the
// right.
IN_PROC_BROWSER_TEST_F(DevToolsControllerTest, WebViewLayout) {
  CGFloat offset = 50;
  [controller() setTopContentOffset:offset];

  SetDockSide(DEVTOOLS_DOCK_SIDE_RIGHT);
  AddTabAtIndex(0,
                GURL(chrome::kAboutBlankURL),
                content::PAGE_TRANSITION_TYPED);
  DevToolsWindow::ToggleDevToolsWindow(browser(), DEVTOOLS_TOGGLE_ACTION_SHOW);

  NSView* container_view = [[[controller() splitView] subviews] lastObject];
  NSView* dev_tools_view = [[container_view subviews] lastObject];
  NSView* web_view = [[dev_tools_view subviews] lastObject];

  CGFloat height = NSHeight([[controller() splitView] bounds]);

  EXPECT_EQ(height - offset, NSHeight([web_view bounds]));
  EXPECT_EQ(0, NSMinY([web_view bounds]));

  // Update the offset and verify that the view is resized.
  CGFloat new_offset = 25;
  [controller() setTopContentOffset:new_offset];
  EXPECT_EQ(height - new_offset, NSHeight([web_view bounds]));
  EXPECT_EQ(0, NSMinY([web_view bounds]));
}
