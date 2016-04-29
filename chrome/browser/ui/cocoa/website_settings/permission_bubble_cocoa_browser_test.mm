// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands_mac.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_cocoa.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/website_settings/permission_bubble_browser_test_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#import "testing/gtest_mac.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest, HasLocationBarByDefault) {
  PermissionBubbleCocoa bubble(browser());
  bubble.SetDelegate(test_delegate());
  bubble.Show(requests(), accept_states());
  EXPECT_TRUE([bubble.bubbleController_ hasVisibleLocationBar]);
  bubble.Hide();
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest,
                       BrowserFullscreenHasLocationBar) {
  ui::test::ScopedFakeNSWindowFullscreen faker;

  PermissionBubbleCocoa bubble(browser());
  bubble.SetDelegate(test_delegate());
  bubble.Show(requests(), accept_states());
  EXPECT_TRUE([bubble.bubbleController_ hasVisibleLocationBar]);

  FullscreenController* controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  controller->ToggleBrowserFullscreenMode();
  faker.FinishTransition();

  // The location bar should be visible if the toolbar is set to be visible in
  // fullscreen mode.
  PrefService* prefs = browser()->profile()->GetPrefs();
  bool show_toolbar = prefs->GetBoolean(prefs::kShowFullscreenToolbar);
  EXPECT_EQ(show_toolbar, [bubble.bubbleController_ hasVisibleLocationBar]);

  // Toggle the value of the preference.
  chrome::ToggleFullscreenToolbar(browser());
  EXPECT_EQ(!show_toolbar, [bubble.bubbleController_ hasVisibleLocationBar]);

  // Put the setting back the way it started.
  chrome::ToggleFullscreenToolbar(browser());
  controller->ToggleBrowserFullscreenMode();
  faker.FinishTransition();

  EXPECT_TRUE([bubble.bubbleController_ hasVisibleLocationBar]);
  bubble.Hide();
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest,
                       TabFullscreenHasLocationBar) {
  ui::test::ScopedFakeNSWindowFullscreen faker;

  PermissionBubbleCocoa bubble(browser());
  bubble.SetDelegate(test_delegate());
  bubble.Show(requests(), accept_states());
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
  bubble.Hide();
}

IN_PROC_BROWSER_TEST_F(PermissionBubbleBrowserTest, AppHasNoLocationBar) {
  Browser* app_browser = OpenExtensionAppWindow();
  PermissionBubbleCocoa bubble(app_browser);
  bubble.SetDelegate(test_delegate());
  bubble.Show(requests(), accept_states());
  EXPECT_FALSE([bubble.bubbleController_ hasVisibleLocationBar]);
  bubble.Hide();
}

// http://crbug.com/470724
// Kiosk mode on Mac has a location bar but it shouldn't.
IN_PROC_BROWSER_TEST_F(PermissionBubbleKioskBrowserTest,
                       DISABLED_KioskHasNoLocationBar) {
  PermissionBubbleCocoa bubble(browser());
  bubble.SetDelegate(test_delegate());
  bubble.Show(requests(), accept_states());
  EXPECT_FALSE([bubble.bubbleController_ hasVisibleLocationBar]);
  bubble.Hide();
}
