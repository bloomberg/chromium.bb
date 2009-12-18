// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Carbon/Carbon.h>

#include "chrome/browser/global_keyboard_shortcuts_mac.h"

#include "base/basictypes.h"
#include "chrome/app/chrome_dll_resource.h"

// Basically, there are two kinds of keyboard shortcuts: Ones that should work
// only if the tab contents is focused (BrowserKeyboardShortcut), and ones that
// should work in all other cases (WindowKeyboardShortcut). In the latter case,
// we differentiate between shortcuts that are checked before any other view
// gets the chance to handle them (WindowKeyboardShortcut) or after all views
// had a chance but did not handle the keypress event
// (DelayedWindowKeyboardShortcut)

const KeyboardShortcutData* GetWindowKeyboardShortcutTable
    (size_t* num_entries) {
  static const KeyboardShortcutData keyboard_shortcuts[] = {
    {true,  true,  false, false, kVK_ANSI_RightBracket, IDC_SELECT_NEXT_TAB},
    {false, false, true,  false, kVK_PageDown,      IDC_SELECT_NEXT_TAB},
    {false, false, true,  false, kVK_Tab,           IDC_SELECT_NEXT_TAB},
    {true,  true,  false, false, kVK_ANSI_LeftBracket, IDC_SELECT_PREVIOUS_TAB},
    {false, false, true,  false, kVK_PageUp,        IDC_SELECT_PREVIOUS_TAB},
    {false, true,  true,  false, kVK_Tab,           IDC_SELECT_PREVIOUS_TAB},
    // Cmd-0..8 select the Nth tab, with cmd-9 being "last tab".
    {true,  false, false, false, kVK_ANSI_1,        IDC_SELECT_TAB_0},
    {true,  false, false, false, kVK_ANSI_2,        IDC_SELECT_TAB_1},
    {true,  false, false, false, kVK_ANSI_3,        IDC_SELECT_TAB_2},
    {true,  false, false, false, kVK_ANSI_4,        IDC_SELECT_TAB_3},
    {true,  false, false, false, kVK_ANSI_5,        IDC_SELECT_TAB_4},
    {true,  false, false, false, kVK_ANSI_6,        IDC_SELECT_TAB_5},
    {true,  false, false, false, kVK_ANSI_7,        IDC_SELECT_TAB_6},
    {true,  false, false, false, kVK_ANSI_8,        IDC_SELECT_TAB_7},
    {true,  false, false, false, kVK_ANSI_9,        IDC_SELECT_LAST_TAB},
  };

  *num_entries = arraysize(keyboard_shortcuts);

  return keyboard_shortcuts;
}

const KeyboardShortcutData* GetDelayedWindowKeyboardShortcutTable
    (size_t* num_entries) {
  static const KeyboardShortcutData keyboard_shortcuts[] = {
    {false, false, false, false, kVK_Escape,        IDC_STOP},
  };

  *num_entries = arraysize(keyboard_shortcuts);

  return keyboard_shortcuts;
}

const KeyboardShortcutData* GetBrowserKeyboardShortcutTable
    (size_t* num_entries) {
  static const KeyboardShortcutData keyboard_shortcuts[] = {
    {true,  false, false, false, kVK_LeftArrow,     IDC_BACK},
    {true,  false, false, false, kVK_RightArrow,    IDC_FORWARD},
    {false, false, false, false, kVK_Delete,        IDC_BACK},
    {false, true,  false, false, kVK_Delete,        IDC_FORWARD},
  };

  *num_entries = arraysize(keyboard_shortcuts);

  return keyboard_shortcuts;
}

static int CommandForKeyboardShortcut(
    const KeyboardShortcutData* (*get_keyboard_shortcut_table)(size_t*),
    bool command_key, bool shift_key, bool cntrl_key, bool opt_key,
    int vkey_code) {

  // Scan through keycodes and see if it corresponds to one of the global
  // shortcuts on file.
  //
  // TODO(jeremy): Change this into a hash table once we get enough
  // entries in the array to make a difference.
  size_t num_shortcuts = 0;
  const KeyboardShortcutData *it = get_keyboard_shortcut_table(&num_shortcuts);
  for (size_t i = 0; i < num_shortcuts; ++i, ++it) {
    if (it->command_key == command_key &&
        it->shift_key == shift_key &&
        it->cntrl_key == cntrl_key &&
        it->opt_key == opt_key &&
        it->vkey_code == vkey_code) {
      return it->chrome_command;
    }
  }

  return -1;
}

int CommandForWindowKeyboardShortcut(
    bool command_key, bool shift_key, bool cntrl_key, bool opt_key,
    int vkey_code) {
  return CommandForKeyboardShortcut(GetWindowKeyboardShortcutTable,
                                    command_key, shift_key,
                                    cntrl_key, opt_key, vkey_code);
}

int CommandForDelayedWindowKeyboardShortcut(
    bool command_key, bool shift_key, bool cntrl_key, bool opt_key,
    int vkey_code) {
  return CommandForKeyboardShortcut(GetDelayedWindowKeyboardShortcutTable,
                                    command_key, shift_key,
                                    cntrl_key, opt_key, vkey_code);
}

int CommandForBrowserKeyboardShortcut(
    bool command_key, bool shift_key, bool cntrl_key, bool opt_key,
    int vkey_code) {
  return CommandForKeyboardShortcut(GetBrowserKeyboardShortcutTable,
                                    command_key, shift_key,
                                    cntrl_key, opt_key, vkey_code);
}
