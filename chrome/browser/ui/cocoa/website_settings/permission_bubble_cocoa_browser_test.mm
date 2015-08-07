// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_cocoa.h"
#include "chrome/browser/ui/website_settings/permission_bubble_browser_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "ui/base/cocoa/fullscreen_window_manager.h"

namespace {
class MockPermissionBubbleCocoa : public PermissionBubbleCocoa {
 public:
  MockPermissionBubbleCocoa(Browser* browser)
      : PermissionBubbleCocoa(browser) {}

  MOCK_METHOD0(HasLocationBar, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPermissionBubbleCocoa);
};
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest, HasLocationBarByDefault) {
  PermissionBubbleCocoa bubble(browser());
  bubble.SetDelegate(test_delegate());
  bubble.Show(requests(), accept_states());
  EXPECT_TRUE(bubble.HasLocationBar());
  bubble.Hide();
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest, FullscreenHasLocationBar) {
  PermissionBubbleCocoa bubble(browser());
  bubble.SetDelegate(test_delegate());
  bubble.Show(requests(), accept_states());

  NSWindow* window = browser()->window()->GetNativeWindow();
  base::scoped_nsobject<FullscreenWindowManager> manager(
      [[FullscreenWindowManager alloc] initWithWindow:window
                                        desiredScreen:[NSScreen mainScreen]]);
  EXPECT_TRUE(bubble.HasLocationBar());
  [manager enterFullscreenMode];
  EXPECT_TRUE(bubble.HasLocationBar());
  [manager exitFullscreenMode];
  EXPECT_TRUE(bubble.HasLocationBar());
  bubble.Hide();
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest, AppHasNoLocationBar) {
  Browser* app_browser = OpenExtensionAppWindow();
  PermissionBubbleCocoa bubble(app_browser);
  bubble.SetDelegate(test_delegate());
  bubble.Show(requests(), accept_states());
  EXPECT_FALSE(bubble.HasLocationBar());
  bubble.Hide();
}

// http://crbug.com/470724
// Kiosk mode on Mac has a location bar but it shouldn't.
IN_PROC_BROWSER_TEST_F(PermissionBubbleKioskBrowserTest,
                       DISABLED_KioskHasNoLocationBar) {
  PermissionBubbleCocoa bubble(browser());
  bubble.SetDelegate(test_delegate());
  bubble.Show(requests(), accept_states());
  EXPECT_FALSE(bubble.HasLocationBar());
  bubble.Hide();
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest,
                       AnchorPositionWithLocationBar) {
  testing::NiceMock<MockPermissionBubbleCocoa> bubble(browser());
  bubble.SetDelegate(test_delegate());
  bubble.Show(requests(), accept_states());
  ON_CALL(bubble, HasLocationBar()).WillByDefault(testing::Return(true));

  NSPoint anchor = bubble.GetAnchorPoint();

  // Expected anchor location will be the same as the page info bubble.
  NSWindow* window = browser()->window()->GetNativeWindow();
  BrowserWindowController* controller =
      [BrowserWindowController browserWindowControllerForWindow:window];
  LocationBarViewMac* location_bar_bridge = [controller locationBarBridge];
  NSPoint expected = location_bar_bridge->GetPageInfoBubblePoint();
  expected = [window convertBaseToScreen:expected];
  EXPECT_NSEQ(expected, anchor);
  bubble.Hide();
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest,
                       AnchorPositionWithoutLocationBar) {
  testing::NiceMock<MockPermissionBubbleCocoa> bubble(browser());
  bubble.SetDelegate(test_delegate());
  bubble.Show(requests(), accept_states());
  ON_CALL(bubble, HasLocationBar()).WillByDefault(testing::Return(false));

  NSPoint anchor = bubble.GetAnchorPoint();

  // Expected anchor location will be top center when there's no location bar.
  NSWindow* window = browser()->window()->GetNativeWindow();
  NSRect frame = [window frame];
  NSPoint expected = NSMakePoint(frame.size.width / 2, frame.size.height);
  expected = [window convertBaseToScreen:expected];
  EXPECT_NSEQ(expected, anchor);
  bubble.Hide();
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest,
                       AnchorPositionDifferentWithAndWithoutLocationBar) {
  testing::NiceMock<MockPermissionBubbleCocoa> bubble(browser());
  bubble.SetDelegate(test_delegate());
  bubble.Show(requests(), accept_states());

  ON_CALL(bubble, HasLocationBar()).WillByDefault(testing::Return(true));
  NSPoint withLocationBar = bubble.GetAnchorPoint();

  ON_CALL(bubble, HasLocationBar()).WillByDefault(testing::Return(false));
  NSPoint withoutLocationBar = bubble.GetAnchorPoint();

  // The bubble should be in different places depending if the location bar is
  // available or not.
  EXPECT_NSNE(withLocationBar, withoutLocationBar);
  bubble.Hide();
}
