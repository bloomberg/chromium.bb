// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/one_click_signin_bubble_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/one_click_signin_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#import "testing/gtest_mac.h"

class OneClickSigninBubbleControllerTest : public InProcessBrowserTest {
 public:
  OneClickSigninBubbleControllerTest()
    : controller_(NULL),
      callback_count_(0) {
  }

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    BrowserWindow::StartSyncCallback callback = base::Bind(
        &OneClickSigninBubbleControllerTest::OnStartSyncCallback,
        base::Unretained(this));

    BrowserWindowCocoa* browser_window =
        static_cast<BrowserWindowCocoa*>(browser()->window());
    controller_.reset([[OneClickSigninBubbleController alloc]
            initWithBrowserWindowController:browser_window->cocoa_controller()
                                webContents:web_contents
                               errorMessage:nil
                                   callback:callback]);
    [controller_ showWindow:nil];
    EXPECT_NSEQ(@"OneClickSigninBubble",
                [[controller_ viewController] nibName]);
  }

  base::scoped_nsobject<OneClickSigninBubbleController> controller_;
  int callback_count_;

 private:
  void OnStartSyncCallback(OneClickSigninSyncStarter::StartSyncMode mode) {
    ++callback_count_;
  }

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninBubbleControllerTest);
};

// Test that the bubble does not start sync if the OK button is clicked.
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleControllerTest, OK) {
  [[controller_ viewController] ok:nil];
  EXPECT_EQ(0, callback_count_);
}

// Test that the bubble does not start sync if the Undo button is clicked.
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleControllerTest, Undo) {
  [[controller_ viewController] onClickUndo:nil];
  EXPECT_EQ(0, callback_count_);
}

// Test that the bubble does not start sync if the bubble is closed.
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleControllerTest, Close) {
  [controller_ close];
  EXPECT_EQ(0, callback_count_);
}

// Test that the advanced page is opened in the current tab and that
// the bubble does not start sync.
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleControllerTest, Advanced) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  [[controller_ viewController] onClickAdvancedLink:nil];
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, callback_count_);
}

// Test that clicking the learn more link opens a new tab and that
// the bubble does not start sync.
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleControllerTest, LearnMore) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  [[controller_ viewController] textView:nil
                           clickedOnLink:nil
                                 atIndex:0];
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, callback_count_);
}
