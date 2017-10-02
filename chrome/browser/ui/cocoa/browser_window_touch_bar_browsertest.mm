// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_touch_bar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest_mac.h"

@interface TestingBrowserWindowTouchBar : BrowserWindowTouchBar

@property(nonatomic, assign) BOOL hasUpdatedReloadStop;
@property(nonatomic, assign) BOOL hasUpdatedBackForward;
@property(nonatomic, assign) BOOL hasUpdatedStarred;

- (void)updateReloadStopButton;
- (void)updateBackForwardControl;
- (void)updateStarredButton;

@end

@implementation TestingBrowserWindowTouchBar

@synthesize hasUpdatedReloadStop = hasUpdatedReloadStop_;
@synthesize hasUpdatedBackForward = hasUpdatedBackForward_;
@synthesize hasUpdatedStarred = hasUpdatedStarred_;

- (void)updateReloadStopButton {
  [super updateReloadStopButton];
  hasUpdatedReloadStop_ = YES;
}

- (void)updateBackForwardControl {
  [super updateBackForwardControl];
  hasUpdatedBackForward_ = YES;
}

- (void)updateStarredButton {
  [super updateStarredButton];
  hasUpdatedStarred_ = YES;
}

- (void)resetFlags {
  hasUpdatedReloadStop_ = YES;
  hasUpdatedBackForward_ = YES;
  hasUpdatedStarred_ = YES;
}

@end

class BrowserWindowTouchBarTest : public InProcessBrowserTest {
 public:
  BrowserWindowTouchBarTest() : InProcessBrowserTest() {}

  void SetUpOnMainThread() override {
    // Ownership is passed to BrowserWindowController in
    // -setBrowserWindowTouchBar:
    browser_touch_bar_ = [[TestingBrowserWindowTouchBar alloc]
                initWithBrowser:browser()
        browserWindowController:browser_window_controller()];
    [browser_window_controller() setBrowserWindowTouchBar:browser_touch_bar_];
  }

  BrowserWindowController* browser_window_controller() {
    return [BrowserWindowController
        browserWindowControllerForWindow:browser()
                                             ->window()
                                             ->GetNativeWindow()];
  }

  TestingBrowserWindowTouchBar* browser_touch_bar() const {
    return browser_touch_bar_;
  }

 private:
  TestingBrowserWindowTouchBar* browser_touch_bar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowTouchBarTest);
};

// Test if the proper controls gets updated when the page loads.
IN_PROC_BROWSER_TEST_F(BrowserWindowTouchBarTest, PageLoadInvalidate) {
  if (@available(macOS 10.12.2, *)) {
    NSButton* reload_stop = [browser_touch_bar() reloadStopButton];
    EXPECT_TRUE(reload_stop);

    BrowserWindowController* bwc = browser_window_controller();
    [bwc setIsLoading:YES force:NO];
    EXPECT_TRUE([browser_touch_bar() hasUpdatedReloadStop]);
    EXPECT_TRUE([browser_touch_bar() hasUpdatedBackForward]);
    EXPECT_TRUE([browser_touch_bar() hasUpdatedStarred]);
    EXPECT_EQ(IDC_STOP, [reload_stop tag]);

    [browser_touch_bar() resetFlags];

    [bwc setIsLoading:NO force:NO];
    EXPECT_TRUE([browser_touch_bar() hasUpdatedReloadStop]);
    EXPECT_TRUE([browser_touch_bar() hasUpdatedBackForward]);
    EXPECT_TRUE([browser_touch_bar() hasUpdatedStarred]);
    EXPECT_EQ(IDC_RELOAD, [reload_stop tag]);
  }
}

// Test if the touch bar gets invalidated when the active tab is changed.
IN_PROC_BROWSER_TEST_F(BrowserWindowTouchBarTest, TabChanges) {
  if (@available(macOS 10.12.2, *)) {
    NSWindow* window = [browser_window_controller() window];
    NSTouchBar* touch_bar = [browser_touch_bar() makeTouchBar];
    [window setTouchBar:touch_bar];
    EXPECT_TRUE([window touchBar]);

    // The window should have a new touch bar.
    [browser_window_controller() onActiveTabChanged:nullptr to:nullptr];
    EXPECT_NE(touch_bar, [window touchBar]);
  }
}

// Test if the touch bar gets invalidated when the starred state is changed.
IN_PROC_BROWSER_TEST_F(BrowserWindowTouchBarTest, StarredChanges) {
  if (@available(macOS 10.12.2, *)) {
    NSWindow* window = [browser_window_controller() window];
    NSTouchBar* touch_bar = [browser_touch_bar() makeTouchBar];
    [window setTouchBar:touch_bar];
    EXPECT_TRUE([window touchBar]);

    // The window should have a new touch bar.
    [browser_window_controller() setStarredState:YES];
    EXPECT_NE(touch_bar, [window touchBar]);
  }
}
