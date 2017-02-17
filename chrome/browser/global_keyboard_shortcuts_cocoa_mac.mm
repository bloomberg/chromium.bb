// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"

// Basically, there are two kinds of keyboard shortcuts: Ones that should work
// only if the tab contents is focused (BrowserKeyboardShortcut), and ones that
// should work in all other cases (WindowKeyboardShortcut). In the latter case,
// we differentiate between shortcuts that are checked before any other view
// gets the chance to handle them (WindowKeyboardShortcut) or after all views
// had a chance but did not handle the keypress event
// (DelayedWindowKeyboardShortcut).

const std::vector<KeyboardShortcutData>& GetWindowKeyboardShortcutTable() {
  // clang-format off
  CR_DEFINE_STATIC_LOCAL(std::vector<KeyboardShortcutData>, result, ({
    //cmd   shift  cntrl  option vkeycode      char command
    //---   -----  -----  ------ --------      ---- -------
    // '{' / '}' characters should be matched earlier than virtual key codes
    // (so we can match alt-8 as '{' on German keyboards).
    {true,  false, false, false, 0,            '}', IDC_SELECT_NEXT_TAB},
    {true,  false, false, false, 0,            '{', IDC_SELECT_PREVIOUS_TAB},
    {false, false, true,  false, kVK_PageDown, 0,   IDC_SELECT_NEXT_TAB},
    {false, false, true,  false, kVK_Tab,      0,   IDC_SELECT_NEXT_TAB},
    {false, false, true,  false, kVK_PageUp,   0,   IDC_SELECT_PREVIOUS_TAB},
    {false, true,  true,  false, kVK_Tab,      0,   IDC_SELECT_PREVIOUS_TAB},

    //cmd  shift  cntrl  option vkeycode          char command
    //---  -----  -----  ------ --------          ---- -------
    // Cmd-0..8 select the nth tab, with cmd-9 being "last tab".
    {true, false, false, false, kVK_ANSI_1,       0,   IDC_SELECT_TAB_0},
    {true, false, false, false, kVK_ANSI_Keypad1, 0,   IDC_SELECT_TAB_0},
    {true, false, false, false, kVK_ANSI_2,       0,   IDC_SELECT_TAB_1},
    {true, false, false, false, kVK_ANSI_Keypad2, 0,   IDC_SELECT_TAB_1},
    {true, false, false, false, kVK_ANSI_3,       0,   IDC_SELECT_TAB_2},
    {true, false, false, false, kVK_ANSI_Keypad3, 0,   IDC_SELECT_TAB_2},
    {true, false, false, false, kVK_ANSI_4,       0,   IDC_SELECT_TAB_3},
    {true, false, false, false, kVK_ANSI_Keypad4, 0,   IDC_SELECT_TAB_3},
    {true, false, false, false, kVK_ANSI_5,       0,   IDC_SELECT_TAB_4},
    {true, false, false, false, kVK_ANSI_Keypad5, 0,   IDC_SELECT_TAB_4},
    {true, false, false, false, kVK_ANSI_6,       0,   IDC_SELECT_TAB_5},
    {true, false, false, false, kVK_ANSI_Keypad6, 0,   IDC_SELECT_TAB_5},
    {true, false, false, false, kVK_ANSI_7,       0,   IDC_SELECT_TAB_6},
    {true, false, false, false, kVK_ANSI_Keypad7, 0,   IDC_SELECT_TAB_6},
    {true, false, false, false, kVK_ANSI_8,       0,   IDC_SELECT_TAB_7},
    {true, false, false, false, kVK_ANSI_Keypad8, 0,   IDC_SELECT_TAB_7},
    {true, false, false, false, kVK_ANSI_9,       0,   IDC_SELECT_LAST_TAB},
    {true, false, false, false, kVK_ANSI_Keypad9, 0,   IDC_SELECT_LAST_TAB},
    {true, true,  false, false, kVK_ANSI_M,       0,   IDC_SHOW_AVATAR_MENU},
    {true, false, false, true,  kVK_ANSI_L,       0,   IDC_SHOW_DOWNLOADS},
  }));
  // clang-format on
  return result;
}

const std::vector<KeyboardShortcutData>&
GetDelayedWindowKeyboardShortcutTable() {
  // clang-format off
  CR_DEFINE_STATIC_LOCAL(std::vector<KeyboardShortcutData>, result, ({
    //cmd   shift  cntrl  option vkeycode    char command
    //---   -----  -----  ------ --------    ---- -------
    {false, false, false, false, kVK_Escape, 0,   IDC_STOP},
  }));
  // clang-format on
  return result;
}

const std::vector<KeyboardShortcutData>& GetBrowserKeyboardShortcutTable() {
  // clang-format off
  CR_DEFINE_STATIC_LOCAL(std::vector<KeyboardShortcutData>, result, ({
    //cmd   shift  cntrl  option vkeycode        char command
    //---   -----  -----  ------ --------        ---- -------
    {true,  false, false, false, kVK_LeftArrow,  0,   IDC_BACK},
    {true,  false, false, false, kVK_RightArrow, 0,   IDC_FORWARD},
    {false, false, false, false, kVK_Delete,     0,   IDC_BACKSPACE_BACK},
    {false, true,  false, false, kVK_Delete,     0,   IDC_BACKSPACE_FORWARD},
    {true,  true,  false, false, 0,              'c', IDC_DEV_TOOLS_INSPECT},
  }));
  // clang-format on
  return result;
}
