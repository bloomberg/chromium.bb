// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/debug/debugger.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/chrome_browser_window.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

// An NSWindowDelegate that implements -commandDispatch:.
@interface TestDelegateWithCommandDispatch : NSObject {
}
- (void)commandDispatch:(id)sender;
@end
@implementation TestDelegateWithCommandDispatch
- (void)commandDispatch:(id)sender {
}
@end

// An NSWindowDelegate that doesn't implement -commandDispatch:.
@interface TestDelegateWithoutCommandDispatch : NSObject {
}
@end
@implementation TestDelegateWithoutCommandDispatch
@end

class ChromeBrowserWindowTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    // Create a window.
    const NSUInteger mask = NSTitledWindowMask | NSClosableWindowMask |
        NSMiniaturizableWindowMask | NSResizableWindowMask;
    window_ = [[ChromeBrowserWindow alloc]
               initWithContentRect:NSMakeRect(0, 0, 800, 600)
               styleMask:mask
               backing:NSBackingStoreBuffered
               defer:NO];
    if (base::debug::BeingDebugged()) {
      [window_ orderFront:nil];
    } else {
      [window_ orderBack:nil];
    }
  }

  virtual void TearDown() {
    [window_ close];
    CocoaTest::TearDown();
  }

  ChromeBrowserWindow* window_;
};

// Baseline test that the window creates, displays, closes, and
// releases.
TEST_F(ChromeBrowserWindowTest, ShowAndClose) {
  [window_ display];
}

// Tests routing of -performClose:.
TEST_F(ChromeBrowserWindowTest, PerformCloseTest) {
  scoped_nsobject<NSMenuItem> zoom_item(
      [[NSMenuItem alloc] initWithTitle:@"Zoom"
                                 action:@selector(performZoom:)
                          keyEquivalent:@""]);
  scoped_nsobject<NSMenuItem> close_item(
      [[NSMenuItem alloc] initWithTitle:@"Close"
                                 action:@selector(performClose:)
                          keyEquivalent:@""]);
  scoped_nsobject<NSMenuItem> close_tab_item(
      [[NSMenuItem alloc] initWithTitle:@"Close Tab"
                                 action:@selector(performClose:)
                          keyEquivalent:@""]);
  [close_tab_item setTag:IDC_CLOSE_TAB];

  scoped_nsobject<id> delegate1(
      [[TestDelegateWithCommandDispatch alloc] init]);
  scoped_nsobject<id> delegate2(
      [[TestDelegateWithoutCommandDispatch alloc] init]);

  struct {
    id delegate;
    id sender;
    BOOL should_route;
  } cases[] = {
    { delegate1, delegate1, NO },
    { delegate1, window_, NO },
    { delegate1, zoom_item, NO },
    { delegate1, close_item, NO },
    { delegate1, close_tab_item, YES },
    { delegate2, close_tab_item, NO },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    [window_ setDelegate:cases[i].delegate];
    EXPECT_EQ(cases[i].should_route,
        [window_ performCloseShouldRouteToCommandDispatch:cases[i].sender]);
  }
}
