// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_cocoa.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_controller.h"
#include "chrome/browser/ui/website_settings/permission_bubble_browser_test_util.h"
#import "testing/gtest_mac.h"
#import "ui/base/cocoa/fullscreen_window_manager.h"

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest, HasLocationBarByDefault) {
  PermissionBubbleCocoa bubble(browser());
  bubble.SetDelegate(test_delegate());
  bubble.Show(requests(), accept_states());
  EXPECT_TRUE([bubble.bubbleController_ hasLocationBar]);
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
  EXPECT_TRUE([bubble.bubbleController_ hasLocationBar]);
  [manager enterFullscreenMode];
  EXPECT_TRUE([bubble.bubbleController_ hasLocationBar]);
  [manager exitFullscreenMode];
  EXPECT_TRUE([bubble.bubbleController_ hasLocationBar]);
  bubble.Hide();
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest, AppHasNoLocationBar) {
  Browser* app_browser = OpenExtensionAppWindow();
  PermissionBubbleCocoa bubble(app_browser);
  bubble.SetDelegate(test_delegate());
  bubble.Show(requests(), accept_states());
  EXPECT_FALSE([bubble.bubbleController_ hasLocationBar]);
  bubble.Hide();
}

// http://crbug.com/470724
// Kiosk mode on Mac has a location bar but it shouldn't.
IN_PROC_BROWSER_TEST_F(PermissionBubbleKioskBrowserTest,
                       DISABLED_KioskHasNoLocationBar) {
  PermissionBubbleCocoa bubble(browser());
  bubble.SetDelegate(test_delegate());
  bubble.Show(requests(), accept_states());
  EXPECT_FALSE([bubble.bubbleController_ hasLocationBar]);
  bubble.Hide();
}
