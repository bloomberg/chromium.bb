// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"

// Lists shortcuts that are impossible to migrate to accelerator_table.cc
// (crbug.com/25946).

const std::vector<KeyboardShortcutData>& GetWindowKeyboardShortcutTable() {
  // clang-format off
  CR_DEFINE_STATIC_LOCAL(std::vector<KeyboardShortcutData>, result, ({
    //cmd  shift  cntrl  option vkeycode char command
    //---  -----  -----  ------ -------- ---- -------
    // '{' / '}' characters should be matched earlier than virtual key codes
    // (so we can match alt-8 as '{' on German keyboards).
    {true, false, false, false, 0,       '}', IDC_SELECT_NEXT_TAB},
    {true, false, false, false, 0,       '{', IDC_SELECT_PREVIOUS_TAB},
  }));
  // clang-format on
  return result;
}

const std::vector<KeyboardShortcutData>&
GetDelayedWindowKeyboardShortcutTable() {
  CR_DEFINE_STATIC_LOCAL(std::vector<KeyboardShortcutData>, result, ({}));
  return result;
}

const std::vector<KeyboardShortcutData>& GetBrowserKeyboardShortcutTable() {
  CR_DEFINE_STATIC_LOCAL(std::vector<KeyboardShortcutData>, result, ({}));
  return result;
}
