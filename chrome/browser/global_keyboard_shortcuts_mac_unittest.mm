// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <AppKit/NSEvent.h>
#include <Carbon/Carbon.h>

#include "chrome/browser/global_keyboard_shortcuts_mac.h"

#include "chrome/app/chrome_dll_resource.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(GlobalKeyboardShortcuts, ShortcutsToWindowCommand) {
  // Test that an invalid shortcut translates into an invalid command id.
  ASSERT_EQ(
      -1, CommandForWindowKeyboardShortcut(false, false, false, false, 0, 0));

  // Check that all known keyboard shortcuts return valid results.
  size_t num_shortcuts = 0;
  const KeyboardShortcutData *it =
      GetWindowKeyboardShortcutTable(&num_shortcuts);
  ASSERT_GT(num_shortcuts, 0U);
  for (size_t i = 0; i < num_shortcuts; ++i, ++it) {
    int cmd_num = CommandForWindowKeyboardShortcut(
        it->command_key, it->shift_key, it->cntrl_key, it->opt_key,
        it->vkey_code, it->key_char);
    ASSERT_EQ(cmd_num, it->chrome_command);
  }

  // Test that cmd-left and backspace are not window-level commands (else they
  // would be invoked even if e.g. the omnibox had focus, where they really
  // should have text editing functionality).
  ASSERT_EQ(-1, CommandForWindowKeyboardShortcut(
      true, false, false, false, kVK_LeftArrow, 0));
  ASSERT_EQ(-1, CommandForWindowKeyboardShortcut(
      false, false, false, false, kVK_Delete, 0));

  // Test that Cmd-'{' and Cmd-'}' are interpreted as IDC_SELECT_NEXT_TAB
  // and IDC_SELECT_PREVIOUS_TAB regardless of the virtual key code values.
  ASSERT_EQ(IDC_SELECT_NEXT_TAB, CommandForWindowKeyboardShortcut(
      true, false, false, false, kVK_ANSI_Period, '}'));
  ASSERT_EQ(IDC_SELECT_PREVIOUS_TAB, CommandForWindowKeyboardShortcut(
      true, true, false, false, kVK_ANSI_Slash, '{'));

  // One more test for Cmd-'{' / Alt-8 (on german keyboard layout).
  ASSERT_EQ(IDC_SELECT_PREVIOUS_TAB, CommandForWindowKeyboardShortcut(
      true, false, false, true, kVK_ANSI_8, '{'));
}

TEST(GlobalKeyboardShortcuts, ShortcutsToDelayedWindowCommand) {
  // Test that an invalid shortcut translates into an invalid command id.
  ASSERT_EQ(-1,
      CommandForDelayedWindowKeyboardShortcut(false, false, false, false,
                                              0, 0));

  // Check that all known keyboard shortcuts return valid results.
  size_t num_shortcuts = 0;
  const KeyboardShortcutData *it =
      GetDelayedWindowKeyboardShortcutTable(&num_shortcuts);
  ASSERT_GT(num_shortcuts, 0U);
  for (size_t i = 0; i < num_shortcuts; ++i, ++it) {
    int cmd_num = CommandForDelayedWindowKeyboardShortcut(
        it->command_key, it->shift_key, it->cntrl_key, it->opt_key,
        it->vkey_code, it->key_char);
    ASSERT_EQ(cmd_num, it->chrome_command);
  }
}

TEST(GlobalKeyboardShortcuts, ShortcutsToBrowserCommand) {
  // Test that an invalid shortcut translates into an invalid command id.
  ASSERT_EQ(
      -1, CommandForBrowserKeyboardShortcut(false, false, false, false,
                                            0, 0));

  // Check that all known keyboard shortcuts return valid results.
  size_t num_shortcuts = 0;
  const KeyboardShortcutData *it =
      GetBrowserKeyboardShortcutTable(&num_shortcuts);
  ASSERT_GT(num_shortcuts, 0U);
  for (size_t i = 0; i < num_shortcuts; ++i, ++it) {
    int cmd_num = CommandForBrowserKeyboardShortcut(
        it->command_key, it->shift_key, it->cntrl_key, it->opt_key,
        it->vkey_code, it->key_char);
    ASSERT_EQ(cmd_num, it->chrome_command);
  }
}

NSEvent* KeyEvent(bool command_key, bool shift_key,
                  bool cntrl_key, bool opt_key,
                  NSString* chars, NSString* charsNoMods) {
  NSUInteger modifierFlags = 0;
  if (command_key)
    modifierFlags |= NSCommandKeyMask;
  if (shift_key)
    modifierFlags |= NSShiftKeyMask;
  if (cntrl_key)
    modifierFlags |= NSControlKeyMask;
  if (opt_key)
    modifierFlags |= NSAlternateKeyMask;
  return [NSEvent keyEventWithType:NSKeyDown
                          location:NSZeroPoint
                     modifierFlags:modifierFlags
                         timestamp:0.0
                      windowNumber:0
                           context:nil
                        characters:chars
       charactersIgnoringModifiers:charsNoMods
                         isARepeat:NO
                           keyCode:0];
}

TEST(GlobalKeyboardShortcuts, KeyCharacterForEvent) {
  // 'a'
  ASSERT_EQ('a', KeyCharacterForEvent(
      KeyEvent(false, false, false, false, @"a", @"a")));
  // cmd-'a' / cmd-shift-'a'
  ASSERT_EQ('a', KeyCharacterForEvent(
      KeyEvent(true,  true,  false, false, @"a", @"A")));
  // '8'
  ASSERT_EQ('8', KeyCharacterForEvent(
      KeyEvent(false, false, false, false, @"8", @"8")));
  // '{' / alt-'8' on german
  ASSERT_EQ('{', KeyCharacterForEvent(
      KeyEvent(false, false, false, true,  @"{", @"8")));
  // cmd-'{' / cmd-shift-'[' on ansi
  ASSERT_EQ('{', KeyCharacterForEvent(
      KeyEvent(true,  true,  false, false, @"[", @"{")));
  // cmd-'z' / cmd-shift-';' on dvorak-qwerty
  ASSERT_EQ('z', KeyCharacterForEvent(
      KeyEvent(true,  true,  false, false, @"z", @":")));
  // Test if getting dead-key events return 0 and do not hang.
  ASSERT_EQ(0,   KeyCharacterForEvent(
      KeyEvent(false, false, false, false, @"",  @"")));
}
