// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::NiceMock;

namespace {

class ConstrainedWindowDelegateMock : public ConstrainedWindowMacDelegate {
 public:
  MOCK_METHOD1(OnConstrainedWindowClosed, void(ConstrainedWindowMac*));
};

}  // namespace

class ConstrainedWindowMacTest : public InProcessBrowserTest {
 public:
  ConstrainedWindowMacTest()
      : InProcessBrowserTest(),
        tab0_(NULL),
        tab1_(NULL),
        controller_(NULL),
        tab_view0_(NULL),
        tab_view1_(NULL) {
    sheet_window_.reset([[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 30, 30)
                  styleMask:NSTitledWindowMask
                    backing:NSBackingStoreBuffered
                      defer:NO]);
    [sheet_window_ setReleasedWhenClosed:NO];
    sheet_.reset([[CustomConstrainedWindowSheet alloc]
        initWithCustomWindow:sheet_window_]);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    AddTabAtIndex(1, GURL("about:blank"), content::PAGE_TRANSITION_LINK);
    tab0_ = browser()->tab_strip_model()->GetWebContentsAt(0);
    tab1_ = browser()->tab_strip_model()->GetWebContentsAt(1);
    EXPECT_EQ(tab1_, browser()->tab_strip_model()->GetActiveWebContents());

    controller_ = [BrowserWindowController browserWindowControllerForWindow:
        browser()->window()->GetNativeWindow()];
    EXPECT_TRUE(controller_);
    tab_view0_ = [[controller_ tabStripController] viewAtIndex:0];
    EXPECT_TRUE(tab_view0_);
    tab_view1_ = [[controller_ tabStripController] viewAtIndex:1];
    EXPECT_TRUE(tab_view1_);
  }

 protected:
  scoped_nsobject<CustomConstrainedWindowSheet> sheet_;
  scoped_nsobject<NSWindow> sheet_window_;
  content::WebContents* tab0_;
  content::WebContents* tab1_;
  BrowserWindowController* controller_;
  NSView* tab_view0_;
  NSView* tab_view1_;
};

// Test that a sheet added to a inactive tab is not shown until the
// tab is activated.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowMacTest, ShowInInactiveTab) {
  // Show dialog in non active tab.
  NiceMock<ConstrainedWindowDelegateMock> delegate;
  ConstrainedWindowMac dialog(&delegate, tab0_, sheet_);
  EXPECT_EQ(0.0, [sheet_window_ alphaValue]);

  // Switch to inactive tab.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_EQ(1.0, [sheet_window_ alphaValue]);

  dialog.CloseWebContentsModalDialog();
}

// If a tab has never been shown then the associated tab view for the web
// content will not be created. Verify that adding a constrained window to such
// a tab works correctly.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowMacTest, ShowInUninitializedTab) {
  scoped_ptr<content::WebContents> web_contents(content::WebContents::Create(
      content::WebContents::CreateParams(browser()->profile())));
  bool was_blocked = false;
  chrome::AddWebContents(browser(), NULL, web_contents.release(),
                         NEW_BACKGROUND_TAB, gfx::Rect(), false, &was_blocked);
  content::WebContents* tab2 =
      browser()->tab_strip_model()->GetWebContentsAt(2);
  ASSERT_TRUE(tab2);
  EXPECT_FALSE([tab2->GetView()->GetNativeView() superview]);

  // Show dialog and verify that it's not visible yet.
  NiceMock<ConstrainedWindowDelegateMock> delegate;
  ConstrainedWindowMac dialog(&delegate, tab2, sheet_);
  EXPECT_FALSE([sheet_window_ isVisible]);

  // Activate the tab and verify that the constrained window is shown.
  browser()->tab_strip_model()->ActivateTabAt(2, true);
  EXPECT_TRUE([tab2->GetView()->GetNativeView() superview]);
  EXPECT_TRUE([sheet_window_ isVisible]);
  EXPECT_EQ(1.0, [sheet_window_ alphaValue]);

  dialog.CloseWebContentsModalDialog();
}

// Test that adding a sheet disables tab dragging.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowMacTest, TabDragging) {
  NiceMock<ConstrainedWindowDelegateMock> delegate;
  ConstrainedWindowMac dialog(&delegate, tab1_, sheet_);

  // Verify that the dialog disables dragging.
  EXPECT_TRUE([controller_ isTabDraggable:tab_view0_]);
  EXPECT_FALSE([controller_ isTabDraggable:tab_view1_]);

  dialog.CloseWebContentsModalDialog();
}

// Test that closing a browser window with a sheet works.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowMacTest, BrowserWindowClose) {
  NiceMock<ConstrainedWindowDelegateMock> delegate;
  ConstrainedWindowMac dialog(&delegate, tab1_, sheet_);
  EXPECT_EQ(1.0, [sheet_window_ alphaValue]);

  // Close the browser window.
  scoped_nsobject<NSWindow> browser_window(
      [browser()->window()->GetNativeWindow() retain]);
  EXPECT_TRUE([browser_window isVisible]);
  [browser()->window()->GetNativeWindow() performClose:nil];
  EXPECT_FALSE([browser_window isVisible]);
}

// Test that closing a tab with a sheet works.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowMacTest, TabClose) {
  NiceMock<ConstrainedWindowDelegateMock> delegate;
  ConstrainedWindowMac dialog(&delegate, tab1_, sheet_);
  EXPECT_EQ(1.0, [sheet_window_ alphaValue]);

  // Close the tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(2, tab_strip->count());
  EXPECT_TRUE(tab_strip->CloseWebContentsAt(1,
                                            TabStripModel::CLOSE_USER_GESTURE));
  EXPECT_EQ(1, tab_strip->count());
}

// Test that adding a sheet disables fullscreen.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowMacTest, Fullscreen) {
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));

  // Dialog will delete it self when closed.
  NiceMock<ConstrainedWindowDelegateMock> delegate;
  ConstrainedWindowMac dialog(&delegate, tab1_, sheet_);

  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));

  dialog.CloseWebContentsModalDialog();
}
