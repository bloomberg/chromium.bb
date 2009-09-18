// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Carbon/Carbon.h>

#include "chrome/browser/global_keyboard_shortcuts_mac.h"

#include "base/basictypes.h"
#include "chrome/app/chrome_dll_resource.h"

const KeyboardShortcutData* GetKeyboardShortCutTable(size_t* num_entries) {
  static const KeyboardShortcutData keyboard_shortcuts[] = {
    {true,  true,  false, kVK_ANSI_RightBracket, IDC_SELECT_NEXT_TAB},
    {false, false, true,  kVK_PageDown,      IDC_SELECT_NEXT_TAB},
    {false, false, true,  kVK_Tab,           IDC_SELECT_NEXT_TAB},
    {true,  true,  false, kVK_ANSI_LeftBracket, IDC_SELECT_PREVIOUS_TAB},
    {false, false, true,  kVK_PageUp,        IDC_SELECT_PREVIOUS_TAB},
    {false, true,  true,  kVK_Tab,           IDC_SELECT_PREVIOUS_TAB},
    // Cmd-0..8 select the Nth tab, with cmd-9 being "last tab".
    {true, false,  false, kVK_ANSI_1,        IDC_SELECT_TAB_0},
    {true, false,  false, kVK_ANSI_2,        IDC_SELECT_TAB_1},
    {true, false,  false, kVK_ANSI_3,        IDC_SELECT_TAB_2},
    {true, false,  false, kVK_ANSI_4,        IDC_SELECT_TAB_3},
    {true, false,  false, kVK_ANSI_5,        IDC_SELECT_TAB_4},
    {true, false,  false, kVK_ANSI_6,        IDC_SELECT_TAB_5},
    {true, false,  false, kVK_ANSI_7,        IDC_SELECT_TAB_6},
    {true, false,  false, kVK_ANSI_8,        IDC_SELECT_TAB_7},
    {true, false,  false, kVK_ANSI_9,        IDC_SELECT_LAST_TAB},
    // TODO(pinkerton): These can't live here yet, they need to be plumbed
    // through the renderer first so it can override if in a text field.
    // http://crbug.com/12557
    // {true, false, false, kVK_LeftArrow,      IDC_BACK},
    // {true, false, false, kVK_RightArrow,     IDC_FORWARD},
  };

  *num_entries = arraysize(keyboard_shortcuts);

  return keyboard_shortcuts;
}

int CommandForKeyboardShortcut(bool command_key, bool shift_key, bool cntrl_key,
    int vkey_code) {

  // Scan through keycodes and see if it corresponds to one of the global
  // shortcuts on file.
  //
  // TODO(jeremy): Change this into a hash table once we get enough
  // entries in the array to make a difference.
  size_t num_shortcuts = 0;
  const KeyboardShortcutData *it = GetKeyboardShortCutTable(&num_shortcuts);
  for (size_t i = 0; i < num_shortcuts; ++i, ++it) {
    if (it->command_key == command_key &&
        it->shift_key == shift_key &&
        it->cntrl_key == cntrl_key &&
        it->vkey_code == vkey_code) {
      return it->chrome_command;
    }
  }

  return -1;
}
