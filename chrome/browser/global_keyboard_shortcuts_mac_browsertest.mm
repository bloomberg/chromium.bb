// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/global_keyboard_shortcuts_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/run_loop.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views_mode_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#import "ui/events/test/cocoa_test_event_utils.h"

using cocoa_test_event_utils::SynthesizeKeyEvent;

class GlobalKeyboardShortcutsTest : public InProcessBrowserTest {
 public:
  GlobalKeyboardShortcutsTest() = default;
  void SetUpOnMainThread() override {
    // Many hotkeys are defined by the main menu. The value of these hotkeys
    // depends on the focused window. We must focus the browser window. This is
    // also why this test must be an interactive_ui_test rather than a browser
    // test.
    ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
        browser()->window()->GetNativeWindow()));
  }
};

namespace {

void ActivateAccelerator(NSWindow* window, NSEvent* ns_event) {
  if ([window performKeyEquivalent:ns_event])
    return;

  // This is consistent with the way AppKit dispatches events when
  // -performKeyEquivalent: returns NO. See "The Path of Key Events" in the
  // Cocoa Event Architecture documentation.
  [window sendEvent:ns_event];
}

}  // namespace

// Test that global keyboard shortcuts are handled by the native window.
IN_PROC_BROWSER_TEST_F(GlobalKeyboardShortcutsTest, SwitchTabsMac) {
  NSWindow* ns_window = browser()->window()->GetNativeWindow();
  TabStripModel* tab_strip = browser()->tab_strip_model();

  // Set up window with 2 tabs.
  chrome::NewTab(browser());
  EXPECT_EQ(2, tab_strip->count());
  EXPECT_TRUE(tab_strip->IsTabSelected(1));

  // Ctrl+Tab goes to the next tab, which loops back to the first tab.
  ActivateAccelerator(
      ns_window,
      SynthesizeKeyEvent(ns_window, true, ui::VKEY_TAB, NSControlKeyMask));
  EXPECT_TRUE(tab_strip->IsTabSelected(0));

  // Cmd+2 goes to the second tab.
  ActivateAccelerator(ns_window, SynthesizeKeyEvent(ns_window, true, ui::VKEY_2,
                                                    NSCommandKeyMask));
  EXPECT_TRUE(tab_strip->IsTabSelected(1));

  // Cmd+{ goes to the previous tab.
  ActivateAccelerator(ns_window,
                      SynthesizeKeyEvent(ns_window, true, ui::VKEY_OEM_4,
                                         NSShiftKeyMask | NSCommandKeyMask));
  EXPECT_TRUE(tab_strip->IsTabSelected(0));
}

// Test that common hotkeys for editing the omnibox work.
IN_PROC_BROWSER_TEST_F(GlobalKeyboardShortcutsTest, CopyPasteOmnibox) {
  BrowserWindow* window = browser()->window();
  ASSERT_TRUE(window);
  LocationBar* location_bar = window->GetLocationBar();
  ASSERT_TRUE(location_bar);
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  ASSERT_TRUE(omnibox_view);

  NSWindow* ns_window = browser()->window()->GetNativeWindow();

  // Cmd+L focuses the omnibox and selects all the text.
  ActivateAccelerator(
      ns_window, SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_L,
                                    NSCommandKeyMask));

  // The first typed letter overrides the existing contents.
  [ns_window
      sendEvent:SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_A,
                                   /*flags=*/0)];
  // The second typed letter just appends.
  [ns_window
      sendEvent:SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_B,
                                   /*flags=*/0)];
  ASSERT_EQ(omnibox_view->GetText(), base::ASCIIToUTF16("ab"));

  // Cmd+A selects the contents.
  ActivateAccelerator(
      ns_window, SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_A,
                                    NSCommandKeyMask));

  // Cmd+C copies the contents.
  ActivateAccelerator(
      ns_window, SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_C,
                                    NSCommandKeyMask));

  // Right arrow moves to the end.
  [ns_window
      sendEvent:SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_RIGHT,
                                   /*flags=*/0)];

  // Cmd+V pastes the contents.
  ActivateAccelerator(
      ns_window, SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_V,
                                    NSCommandKeyMask));
  EXPECT_EQ(omnibox_view->GetText(), base::ASCIIToUTF16("abab"));
}

// Tests that the shortcut to reopen a previous tab works.
IN_PROC_BROWSER_TEST_F(GlobalKeyboardShortcutsTest, ReopenPreviousTab) {
  NSWindow* ns_window = browser()->window()->GetNativeWindow();
  TabStripModel* tab_strip = browser()->tab_strip_model();

  // Set up window with 2 tabs.
  chrome::NewTab(browser());
  EXPECT_EQ(2, tab_strip->count());

  // Navigate the active tab to a dummy URL.
  GURL test_url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("title1.html")));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), test_url,
      /*number_of_navigations=*/1);
  ASSERT_EQ(tab_strip->GetActiveWebContents()->GetLastCommittedURL(), test_url);

  // Close a tab.
  content::WindowedNotificationObserver wait_for_closed_tab(
      chrome::NOTIFICATION_TAB_CLOSING,
      content::NotificationService::AllSources());
  ActivateAccelerator(
      ns_window, SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_W,
                                    NSCommandKeyMask));
  wait_for_closed_tab.Wait();
  EXPECT_EQ(1, tab_strip->count());
  ASSERT_NE(tab_strip->GetActiveWebContents()->GetLastCommittedURL(), test_url);

  // Reopen a tab.
  content::WindowedNotificationObserver wait_for_added_tab(
      chrome::NOTIFICATION_TAB_PARENTED,
      content::NotificationService::AllSources());
  ActivateAccelerator(
      ns_window, SynthesizeKeyEvent(ns_window, /*keydown=*/true, ui::VKEY_T,
                                    NSCommandKeyMask | NSShiftKeyMask));
  wait_for_added_tab.Wait();
  EXPECT_EQ(2, tab_strip->count());
  ASSERT_EQ(tab_strip->GetActiveWebContents()->GetLastCommittedURL(), test_url);
}

// Checks that manually configured hotkeys in the main menu have higher priority
// than unconfigurable hotkeys not present in the main menu.
IN_PROC_BROWSER_TEST_F(GlobalKeyboardShortcutsTest, MenuCommandPriority) {
  NSWindow* ns_window = browser()->window()->GetNativeWindow();
  TabStripModel* tab_strip = browser()->tab_strip_model();

  // Set up window with 4 tabs.
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  EXPECT_EQ(4, tab_strip->count());
  EXPECT_TRUE(tab_strip->IsTabSelected(3));

  // Use the cmd-2 hotkey to switch to the second tab.
  ActivateAccelerator(ns_window, SynthesizeKeyEvent(ns_window, true, ui::VKEY_2,
                                                    NSCommandKeyMask));
  EXPECT_TRUE(tab_strip->IsTabSelected(1));

  // Change the "Select Next Tab" menu item's key equivalent to be cmd-2, to
  // simulate what would happen if there was a user key equivalent for it. Note
  // that there is a readonly "userKeyEquivalent" property on NSMenuItem, but
  // this code can't modify it.
  NSMenu* main_menu = [NSApp mainMenu];
  ASSERT_NE(nil, main_menu);
  NSMenuItem* window_menu = [main_menu itemWithTitle:@"Window"];
  ASSERT_NE(nil, window_menu);
  ASSERT_TRUE(window_menu.hasSubmenu);
  NSMenuItem* next_item = [window_menu.submenu itemWithTag:IDC_SELECT_NEXT_TAB];
  ASSERT_NE(nil, next_item);
  [next_item setKeyEquivalent:@"2"];
  [next_item setKeyEquivalentModifierMask:NSCommandKeyMask];
  ASSERT_TRUE([next_item isEnabled]);

  // Send cmd-2 again, and ensure the tab switches.
  ActivateAccelerator(ns_window, SynthesizeKeyEvent(ns_window, true, ui::VKEY_2,
                                                    NSCommandKeyMask));
  EXPECT_TRUE(tab_strip->IsTabSelected(2));
  ActivateAccelerator(ns_window, SynthesizeKeyEvent(ns_window, true, ui::VKEY_2,
                                                    NSCommandKeyMask));
  EXPECT_TRUE(tab_strip->IsTabSelected(3));
}
