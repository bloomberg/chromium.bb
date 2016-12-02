// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/global_keyboard_shortcuts_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#import "ui/events/test/cocoa_test_event_utils.h"

using cocoa_test_event_utils::SynthesizeKeyEvent;

using GlobalKeyboardShortcutsTest = ExtensionBrowserTest;

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
