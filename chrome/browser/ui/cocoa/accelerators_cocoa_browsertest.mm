// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest_mac.h"
#include "ui/base/accelerators/platform_accelerator_cocoa.h"
#include "ui/base/l10n/l10n_util_mac.h"
#import "ui/events/keycodes/keyboard_code_conversion_mac.h"

typedef InProcessBrowserTest AcceleratorsCocoaBrowserTest;

namespace {

// Adds all NSMenuItems with an accelerator to the array.
void AddAcceleratorItemsToArray(NSMenu* menu, NSMutableArray* array) {
  for (NSMenuItem* item in [menu itemArray]) {
    NSMenu* submenu = item.submenu;
    if (submenu)
      AddAcceleratorItemsToArray(submenu, array);

    if (item.keyEquivalent.length > 0)
      [array addObject:item];
  }
}

// Returns the NSMenuItem that has the given keyEquivalent and modifiers, or
// nil.
NSMenuItem* MenuContainsAccelerator(NSMenu* menu,
                                    NSString* key_equivalent,
                                    NSUInteger modifiers) {
  for (NSMenuItem* item in [menu itemArray]) {
    NSMenu* submenu = item.submenu;
    if (submenu) {
      NSMenuItem* result =
          MenuContainsAccelerator(submenu, key_equivalent, modifiers);
      if (result)
        return result;
    }

    if ([item.keyEquivalent isEqual:key_equivalent]) {
      // We don't want to ignore shift for [cmd + shift + tab] and [cmd + tab],
      // which are special.
      if (item.tag == IDC_SELECT_NEXT_TAB ||
          item.tag == IDC_SELECT_PREVIOUS_TAB) {
        if (modifiers == item.keyEquivalentModifierMask)
          return item;
        continue;
      }

      BOOL maskEqual =
          (modifiers == item.keyEquivalentModifierMask) ||
          ((modifiers & (~NSShiftKeyMask)) == item.keyEquivalentModifierMask);
      if (maskEqual)
        return item;
    }
  }
  return nil;
}

}  // namespace

// Checks that each NSMenuItem in the main menu has a corresponding accelerator,
// and the keyEquivalent/modifiers match.
IN_PROC_BROWSER_TEST_F(AcceleratorsCocoaBrowserTest,
                       MainMenuAcceleratorsInMapping) {
  NSMenu* menu = [NSApp mainMenu];
  NSMutableArray* array = [NSMutableArray array];
  AddAcceleratorItemsToArray(menu, array);

  for (NSMenuItem* item in array) {
    NSInteger command_id = item.tag;
    AcceleratorsCocoa* keymap = AcceleratorsCocoa::GetInstance();
    const ui::Accelerator* accelerator;

    // If the tag is zero, then either this is a macOS menu item that we don't
    // care about, or it's a chrome accelerator with non-standard selector. We
    // don't have an easy way to distinguish between these, so we just ignore
    // them.
    if (command_id == 0) {
      continue;
    }

    // If the tag isn't zero, then it must correspond to an IDC_* command.
    accelerator = keymap->GetAcceleratorForCommand(command_id);
    EXPECT_TRUE(accelerator);
    if (!accelerator)
      continue;

    // Get the Cocoa key_equivalent associated with the accelerator.
    NSString* keyEquivalent;
    NSUInteger modifierMask;
    GetKeyEquivalentAndModifierMaskFromAccelerator(*accelerator, &keyEquivalent,
                                                   &modifierMask);

    // Check that the menu item's keyEquivalent matches the one from the
    // Cocoa accelerator map.
    EXPECT_NSEQ(keyEquivalent, item.keyEquivalent);

    // Check that the menu item's modifier mask matches the one stored in the
    // accelerator. A mask that include NSShiftKeyMask may not include the
    // relevant bit (the information is reflected in the keyEquivalent of the
    // NSMenuItem).
    BOOL maskEqual =
        (modifierMask == item.keyEquivalentModifierMask) ||
        ((modifierMask & (~NSShiftKeyMask)) == item.keyEquivalentModifierMask);
    EXPECT_TRUE(maskEqual);
  }
}

// Check that each accelerator with a command_id has an associated NSMenuItem
// in the main menu. If the selector is commandDispatch:, then the tag must
// match the command_id.
IN_PROC_BROWSER_TEST_F(AcceleratorsCocoaBrowserTest,
                       MappingAcceleratorsInMainMenu) {
  AcceleratorsCocoa* keymap = AcceleratorsCocoa::GetInstance();
  // The "Share" menu is dynamically populated.
  NSMenu* mainMenu = [NSApp mainMenu];
  NSMenu* fileMenu = [[mainMenu itemWithTag:IDC_FILE_MENU] submenu];
  NSMenu* shareMenu =
      [[fileMenu itemWithTitle:l10n_util::GetNSString(IDS_SHARE_MAC)] submenu];
  [[shareMenu delegate] menuNeedsUpdate:shareMenu];

  for (AcceleratorsCocoa::AcceleratorMap::iterator it =
           keymap->accelerators_.begin();
       it != keymap->accelerators_.end();
       ++it) {
    NSString* keyEquivalent;
    NSUInteger modifierMask;
    GetKeyEquivalentAndModifierMaskFromAccelerator(it->second, &keyEquivalent,
                                                   &modifierMask);

    // Check that there exists a corresponding NSMenuItem.
    NSMenuItem* item =
        MenuContainsAccelerator([NSApp mainMenu], keyEquivalent, modifierMask);
    EXPECT_TRUE(item);

    // If the menu uses a commandDispatch:, the tag must match the command id!
    // Added an exception for IDC_TOGGLE_FULLSCREEN_TOOLBAR, which conflicts
    // with IDC_PRESENTATION_MODE.
    if (item.action == @selector(commandDispatch:)
        && item.tag != IDC_TOGGLE_FULLSCREEN_TOOLBAR)
      EXPECT_EQ(item.tag, it->first);
  }
}
