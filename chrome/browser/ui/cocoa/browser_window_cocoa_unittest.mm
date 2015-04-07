// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_details.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Main test class.
class BrowserWindowCocoaTest : public CocoaProfileTest {
  void SetUp() override {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    controller_ = [[BrowserWindowController alloc] initWithBrowser:browser()
                                                     takeOwnership:NO];
  }

  void TearDown() override {
    [controller_ close];
    CocoaProfileTest::TearDown();
  }

 public:
  BrowserWindowController* controller_;
};

TEST_F(BrowserWindowCocoaTest, TestBookmarkBarVisible) {
  scoped_ptr<BrowserWindowCocoa> bwc(
      new BrowserWindowCocoa(browser(), controller_));

  bool before = bwc->IsBookmarkBarVisible();
  chrome::ToggleBookmarkBarWhenVisible(profile());
  EXPECT_NE(before, bwc->IsBookmarkBarVisible());

  chrome::ToggleBookmarkBarWhenVisible(profile());
  EXPECT_EQ(before, bwc->IsBookmarkBarVisible());
}

// Test that IsMaximized() returns false when the browser window goes from
// maximized to minimized state - http://crbug/452976.
TEST_F(BrowserWindowCocoaTest, TestMinimizeState) {
  scoped_ptr<BrowserWindowCocoa> bwc(
      new BrowserWindowCocoa(browser(), controller_));

  EXPECT_FALSE(bwc->IsMinimized());
  bwc->Maximize();
  EXPECT_TRUE(bwc->IsMaximized());
  EXPECT_FALSE(bwc->IsMinimized());
  bwc->Minimize();
  EXPECT_FALSE(bwc->IsMaximized());
  EXPECT_TRUE(bwc->IsMinimized());
  bwc->Restore();
}

// Tests that BrowserWindowCocoa::Close mimics the behavior of
// -[NSWindow performClose:].
class BrowserWindowCocoaCloseTest : public CocoaProfileTest {
 public:
  BrowserWindowCocoaCloseTest()
      : controller_(
            [OCMockObject mockForClass:[BrowserWindowController class]]),
        window_([OCMockObject mockForClass:[NSWindow class]]) {
    [[[controller_ stub] andReturn:nil] overlayWindow];
  }

  void CreateAndCloseBrowserWindow() {
    BrowserWindowCocoa browser_window(browser(), controller_);
    browser_window.Close();
  }

  id ValueYES() {
    BOOL v = YES;
    return OCMOCK_VALUE(v);
  }
  id ValueNO() {
    BOOL v = NO;
    return OCMOCK_VALUE(v);
  }

 protected:
  id controller_;
  id window_;
};

TEST_F(BrowserWindowCocoaCloseTest, DelegateRespondsYes) {
  [[[window_ stub] andReturn:controller_] delegate];
  [[[controller_ stub] andReturn:window_] window];
  [[[controller_ stub] andReturnValue:ValueYES()] windowShouldClose:window_];
  [[window_ expect] orderOut:nil];
  [[window_ expect] close];
  CreateAndCloseBrowserWindow();
  EXPECT_OCMOCK_VERIFY(controller_);
  EXPECT_OCMOCK_VERIFY(window_);
}

TEST_F(BrowserWindowCocoaCloseTest, DelegateRespondsNo) {
  [[[window_ stub] andReturn:controller_] delegate];
  [[[controller_ stub] andReturn:window_] window];
  [[[controller_ stub] andReturnValue:ValueNO()] windowShouldClose:window_];
  // Window should not be closed.
  CreateAndCloseBrowserWindow();
  EXPECT_OCMOCK_VERIFY(controller_);
  EXPECT_OCMOCK_VERIFY(window_);
}

// NSWindow does not implement |-windowShouldClose:|, but subclasses can
// implement it, and |-performClose:| will invoke it if implemented.
@interface BrowserWindowCocoaCloseWindow : NSWindow
- (BOOL)windowShouldClose:(id)window;
@end
@implementation BrowserWindowCocoaCloseWindow
- (BOOL)windowShouldClose:(id)window {
  return YES;
}
@end

TEST_F(BrowserWindowCocoaCloseTest, WindowRespondsYes) {
  window_ = [OCMockObject mockForClass:[BrowserWindowCocoaCloseWindow class]];
  [[[window_ stub] andReturn:nil] delegate];
  [[[controller_ stub] andReturn:window_] window];
  [[[window_ stub] andReturnValue:ValueYES()] windowShouldClose:window_];
  [[window_ expect] orderOut:nil];
  [[window_ expect] close];
  CreateAndCloseBrowserWindow();
  EXPECT_OCMOCK_VERIFY(controller_);
  EXPECT_OCMOCK_VERIFY(window_);
}

TEST_F(BrowserWindowCocoaCloseTest, WindowRespondsNo) {
  window_ = [OCMockObject mockForClass:[BrowserWindowCocoaCloseWindow class]];
  [[[window_ stub] andReturn:nil] delegate];
  [[[controller_ stub] andReturn:window_] window];
  [[[window_ stub] andReturnValue:ValueNO()] windowShouldClose:window_];
  // Window should not be closed.
  CreateAndCloseBrowserWindow();
  EXPECT_OCMOCK_VERIFY(controller_);
  EXPECT_OCMOCK_VERIFY(window_);
}

TEST_F(BrowserWindowCocoaCloseTest, DelegateRespondsYesWindowRespondsNo) {
  window_ = [OCMockObject mockForClass:[BrowserWindowCocoaCloseWindow class]];
  [[[window_ stub] andReturn:controller_] delegate];
  [[[controller_ stub] andReturn:window_] window];
  [[[controller_ stub] andReturnValue:ValueYES()] windowShouldClose:window_];
  [[[window_ stub] andReturnValue:ValueNO()] windowShouldClose:window_];
  [[window_ expect] orderOut:nil];
  [[window_ expect] close];
  CreateAndCloseBrowserWindow();
  EXPECT_OCMOCK_VERIFY(controller_);
  EXPECT_OCMOCK_VERIFY(window_);
}

TEST_F(BrowserWindowCocoaCloseTest, DelegateRespondsNoWindowRespondsYes) {
  window_ = [OCMockObject mockForClass:[BrowserWindowCocoaCloseWindow class]];
  [[[window_ stub] andReturn:controller_] delegate];
  [[[controller_ stub] andReturn:window_] window];
  [[[controller_ stub] andReturnValue:ValueNO()] windowShouldClose:window_];
  [[[window_ stub] andReturnValue:ValueYES()] windowShouldClose:window_];
  // Window should not be closed.
  CreateAndCloseBrowserWindow();
  EXPECT_OCMOCK_VERIFY(controller_);
  EXPECT_OCMOCK_VERIFY(window_);
}

TEST_F(BrowserWindowCocoaCloseTest, NoResponseFromDelegateNorWindow) {
  [[[window_ stub] andReturn:nil] delegate];
  [[[controller_ stub] andReturn:window_] window];
  [[window_ expect] orderOut:nil];
  [[window_ expect] close];
  CreateAndCloseBrowserWindow();
  EXPECT_OCMOCK_VERIFY(controller_);
  EXPECT_OCMOCK_VERIFY(window_);
}

// TODO(???): test other methods of BrowserWindowCocoa
