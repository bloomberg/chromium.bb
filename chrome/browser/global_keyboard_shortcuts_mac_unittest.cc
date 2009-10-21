// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Carbon/Carbon.h>

#include "chrome/browser/global_keyboard_shortcuts_mac.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(GlobalKeyboardShortcuts, ShortcutsToWindowCommand) {
  // Test that an invalid shortcut translates into an invalid command id.
  ASSERT_EQ(
      -1, CommandForWindowKeyboardShortcut(false, false, false, false, 0));

  // Check that all known keyboard shortcuts return valid results.
  size_t num_shortcuts = 0;
  const KeyboardShortcutData *it =
      GetWindowKeyboardShortcutTable(&num_shortcuts);
  ASSERT_GT(num_shortcuts, 0U);
  for (size_t i = 0; i < num_shortcuts; ++i, ++it) {
    int cmd_num = CommandForWindowKeyboardShortcut(
        it->command_key, it->shift_key, it->cntrl_key, it->opt_key,
        it->vkey_code);
    ASSERT_EQ(cmd_num, it->chrome_command);
  }

  // Test that cmd-left and backspace are not window-level commands (else they
  // would be invoked even if e.g. the omnibox had focus, where they really
  // should have text editing functionality).
  ASSERT_EQ(-1, CommandForWindowKeyboardShortcut(
      true, false, false, false, kVK_LeftArrow));
  ASSERT_EQ(-1, CommandForWindowKeyboardShortcut(
      false, false, false, false, kVK_Delete));
}

TEST(GlobalKeyboardShortcuts, ShortcutsToBrowserCommand) {
  // Test that an invalid shortcut translates into an invalid command id.
  ASSERT_EQ(
      -1, CommandForBrowserKeyboardShortcut(false, false, false, false, 0));

  // Check that all known keyboard shortcuts return valid results.
  size_t num_shortcuts = 0;
  const KeyboardShortcutData *it =
      GetBrowserKeyboardShortcutTable(&num_shortcuts);
  ASSERT_GT(num_shortcuts, 0U);
  for (size_t i = 0; i < num_shortcuts; ++i, ++it) {
    int cmd_num = CommandForBrowserKeyboardShortcut(
        it->command_key, it->shift_key, it->cntrl_key, it->opt_key,
        it->vkey_code);    ASSERT_EQ(cmd_num, it->chrome_command);
  }
}
