// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands_mac.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"
#import "chrome/browser/ui/cocoa/permission_bubble/permission_bubble_cocoa.h"
#import "chrome/browser/ui/cocoa/permission_bubble/permission_bubble_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/permission_bubble/permission_bubble_browser_test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#import "testing/gtest_mac.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest, HasLocationBarByDefault) {
  PermissionBubbleCocoa bubble(browser(), test_delegate());
  EXPECT_TRUE([bubble.bubbleController_ hasVisibleLocationBar]);
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest,
                       BrowserFullscreenHasLocationBar) {
  ui::test::ScopedFakeNSWindowFullscreen faker;

  PermissionBubbleCocoa bubble(browser(), test_delegate());

  // Toggling the toolbar in fullscreen may trigger an animation, but it also
  // depends on the current mouse cursor position which is hard to manipulate in
  // a browser_test. So toggle the toolbar style between PRESENT and NONE using
  // the test API. Don't use HIDDEN, since that would cause flakes depending
  // where the mouse cursor is on the test machine.
  BrowserWindowController* browser_controller = [BrowserWindowController
      browserWindowControllerForWindow:browser()->window()->GetNativeWindow()];

  // Note that outside of fullscreen, there is no fullscreen toolbar controller.
  EXPECT_FALSE([browser_controller fullscreenToolbarController]);

  ExclusiveAccessManager* access_manager =
      browser()->exclusive_access_manager();
  FullscreenController* fullscreen_controller =
      access_manager->fullscreen_controller();

  EXPECT_FALSE(access_manager->context()->IsFullscreen());
  fullscreen_controller->ToggleBrowserFullscreenMode();
  faker.FinishTransition();
  EXPECT_TRUE(access_manager->context()->IsFullscreen());

  FullscreenToolbarController* toolbar_controller =
      [browser_controller fullscreenToolbarController];
  EXPECT_TRUE(toolbar_controller);

  // In fullscreen, try removing the toolbar.
  [toolbar_controller setToolbarStyle:FullscreenToolbarStyle::TOOLBAR_NONE];
  EXPECT_FALSE([bubble.bubbleController_ hasVisibleLocationBar]);
  [toolbar_controller setToolbarStyle:FullscreenToolbarStyle::TOOLBAR_PRESENT];
  EXPECT_TRUE([bubble.bubbleController_ hasVisibleLocationBar]);

  // Leave fullscreen. Toolbar should come back.
  fullscreen_controller->ToggleBrowserFullscreenMode();
  faker.FinishTransition();
  EXPECT_FALSE(access_manager->context()->IsFullscreen());
  EXPECT_FALSE([browser_controller fullscreenToolbarController]);
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest,
                       TabFullscreenHasLocationBar) {
  ui::test::ScopedFakeNSWindowFullscreen faker;

  PermissionBubbleCocoa bubble(browser(), test_delegate());
  EXPECT_TRUE([bubble.bubbleController_ hasVisibleLocationBar]);

  FullscreenController* controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  controller->EnterFullscreenModeForTab(
      browser()->tab_strip_model()->GetActiveWebContents(), GURL());
  faker.FinishTransition();

  EXPECT_FALSE([bubble.bubbleController_ hasVisibleLocationBar]);
  controller->ExitFullscreenModeForTab(
      browser()->tab_strip_model()->GetActiveWebContents());
  faker.FinishTransition();

  EXPECT_TRUE([bubble.bubbleController_ hasVisibleLocationBar]);
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest, AppHasNoLocationBar) {
  Browser* app_browser = OpenExtensionAppWindow();
  PermissionBubbleCocoa bubble(app_browser, test_delegate());
  EXPECT_FALSE([bubble.bubbleController_ hasVisibleLocationBar]);
}

// http://crbug.com/470724
// Kiosk mode on Mac has a location bar but it shouldn't.
IN_PROC_BROWSER_TEST_F(PermissionBubbleKioskBrowserTest,
                       DISABLED_KioskHasNoLocationBar) {
  PermissionBubbleCocoa bubble(browser(), test_delegate());
  EXPECT_FALSE([bubble.bubbleController_ hasVisibleLocationBar]);
}
