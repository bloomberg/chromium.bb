// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/panel.h"
#include "content/public/test/test_utils.h"

typedef BasePanelBrowserTest PanelCocoaBrowserTest;

IN_PROC_BROWSER_TEST_F(PanelCocoaBrowserTest, MenuItems) {
  Panel* panel = CreatePanel("Panel");

  // Close main tabbed window.
  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser()));
  chrome::CloseWindow(browser());
  signal.Wait();

  // There should be no browser windows.
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  // There should be one panel.
  EXPECT_EQ(1, PanelManager::GetInstance()->num_panels());

  NSMenu* mainMenu = [NSApp mainMenu];
  EXPECT_TRUE(mainMenu);

  // Chrome(0) File(1) ....
  // Get File submenu. It doesn't have a command id, fetch it by index.
  NSMenu* fileSubmenu = [[[mainMenu itemArray] objectAtIndex:1] submenu];
  EXPECT_TRUE(fileSubmenu);
  [fileSubmenu update];

  // Verify the items normally enabled for "all windows closed" case are
  // also enabled when there is a panel but no browser windows on the screen.
  NSMenuItem* item = [fileSubmenu itemWithTag:IDC_NEW_TAB];
  EXPECT_TRUE(item);
  EXPECT_TRUE([item isEnabled]);

  item = [fileSubmenu itemWithTag:IDC_NEW_WINDOW];
  EXPECT_TRUE(item);
  EXPECT_TRUE([item isEnabled]);

  item = [fileSubmenu itemWithTag:IDC_NEW_INCOGNITO_WINDOW];
  EXPECT_TRUE(item);
  EXPECT_TRUE([item isEnabled]);

  NSMenu* historySubmenu = [[mainMenu itemWithTag:IDC_HISTORY_MENU] submenu];
  EXPECT_TRUE(historySubmenu);
  [historySubmenu update];

  // These should be disabled.
  item = [historySubmenu itemWithTag:IDC_HOME];
  EXPECT_TRUE(item);
  EXPECT_FALSE([item isEnabled]);

  item = [historySubmenu itemWithTag:IDC_BACK];
  EXPECT_TRUE(item);
  EXPECT_FALSE([item isEnabled]);

  item = [historySubmenu itemWithTag:IDC_FORWARD];
  EXPECT_TRUE(item);
  EXPECT_FALSE([item isEnabled]);

  // 'Close Window' should be enabled because the remaining Panel is a Responder
  // which implements performClose:, the 'action' of 'Close Window'.
  for (NSMenuItem *i in [fileSubmenu itemArray]) {
    if ([i action] == @selector(performClose:)) {
      item = i;
      break;
    }
  }
  EXPECT_TRUE(item);
  EXPECT_TRUE([item isEnabled]);

  panel->Close();
}
