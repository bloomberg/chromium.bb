// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/foundation_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/find_bar/find_bar_text_field.h"
#import "chrome/browser/ui/cocoa/test/run_loop_testing.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/base/test/ui_controls.h"
#import "ui/events/test/cocoa_test_event_utils.h"

namespace {

bool FindBarHasFocus(Browser* browser) {
  NSWindow* window = browser->window()->GetNativeWindow();
  NSText* text_view = base::mac::ObjCCast<NSText>([window firstResponder]);
  return [[text_view delegate] isKindOfClass:[FindBarTextField class]];
}

}  // namespace

typedef InProcessBrowserTest FindBarBrowserTest;

IN_PROC_BROWSER_TEST_F(FindBarBrowserTest, FocusOnTabSwitch) {
  AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK);
  browser()->GetFindBarController()->Show();

  // Verify that if the find bar has focus then switching tabs and changing
  // back sets focus back to the find bar.
  browser()->GetFindBarController()->find_bar()->SetFocusAndSelection();
  EXPECT_TRUE(FindBarHasFocus(browser()));
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_FALSE(FindBarHasFocus(browser()));
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  EXPECT_TRUE(FindBarHasFocus(browser()));

  // Verify that if the find bar does not have focus then switching tabs and
  // changing does not set focus to the find bar.
  browser()->window()->GetLocationBar()->FocusLocation(true);
  EXPECT_FALSE(FindBarHasFocus(browser()));
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_FALSE(FindBarHasFocus(browser()));
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  EXPECT_FALSE(FindBarHasFocus(browser()));
}

IN_PROC_BROWSER_TEST_F(FindBarBrowserTest, EscapeKey) {
  // Enter fullscreen.
  std::unique_ptr<FullscreenNotificationObserver> waiter(
      new FullscreenNotificationObserver());
  browser()
      ->exclusive_access_manager()
      ->fullscreen_controller()
      ->ToggleBrowserFullscreenMode();
  waiter->Wait();

  NSWindow* window = browser()->window()->GetNativeWindow();
  BrowserWindowController* bwc =
      [BrowserWindowController browserWindowControllerForWindow:window];
  EXPECT_TRUE([bwc isInAppKitFullscreen]);

  // Show and focus on the find bar.
  browser()->GetFindBarController()->Show();
  browser()->GetFindBarController()->find_bar()->SetFocusAndSelection();
  EXPECT_TRUE(FindBarHasFocus(browser()));

  // Simulate a key press with the ESC key.
  base::RunLoop run_loop;
  ui_controls::EnableUIControls();
  ui_controls::SendKeyPressNotifyWhenDone(window, ui::VKEY_ESCAPE, false, false,
                                          false, false, run_loop.QuitClosure());
  run_loop.Run();

  // Check that the browser is still in fullscreen and that the find bar has
  // lost focus.
  EXPECT_FALSE(FindBarHasFocus(browser()));
  EXPECT_TRUE([bwc isInAppKitFullscreen]);
}
