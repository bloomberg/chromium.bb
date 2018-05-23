// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accelerator_table.h"

#include <stddef.h>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"

namespace {

// NOTE: Between each ifdef block, keep the list in the same
// (mostly-alphabetical) order as the Windows accelerators in
// ../../app/chrome_dll.rc.
// Do not use Ctrl-Alt as a shortcut modifier, as it is used by i18n keyboards:
// http://blogs.msdn.com/b/oldnewthing/archive/2004/03/29/101121.aspx
// For many commands, the Mac equivalent uses Cmd instead of Ctrl. We only need
// to list the ones that do not have a key equivalent in the main menu, i.e.
// only the ones in global_keyboard_shortcuts_mac.mm.
const AcceleratorMapping kAcceleratorMap[] = {
    {ui::VKEY_D, ui::EF_PLATFORM_ACCELERATOR, IDC_BOOKMARK_PAGE},
    {ui::VKEY_D, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_BOOKMARK_ALL_TABS},
    {ui::VKEY_W, ui::EF_PLATFORM_ACCELERATOR, IDC_CLOSE_TAB},
    {ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_CLOSE_WINDOW},
    {ui::VKEY_F, ui::EF_PLATFORM_ACCELERATOR, IDC_FIND},
    {ui::VKEY_G, ui::EF_PLATFORM_ACCELERATOR, IDC_FIND_NEXT},
    {ui::VKEY_G, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_FIND_PREVIOUS},
    {ui::VKEY_L, ui::EF_PLATFORM_ACCELERATOR, IDC_FOCUS_LOCATION},
    {ui::VKEY_F12, ui::EF_NONE, IDC_DEV_TOOLS_TOGGLE},
    {ui::VKEY_O, ui::EF_PLATFORM_ACCELERATOR, IDC_OPEN_FILE},
    {ui::VKEY_P, ui::EF_PLATFORM_ACCELERATOR, IDC_PRINT},
    {ui::VKEY_R, ui::EF_PLATFORM_ACCELERATOR, IDC_RELOAD},
    {ui::VKEY_R, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_RELOAD_BYPASSING_CACHE},
    {ui::VKEY_S, ui::EF_PLATFORM_ACCELERATOR, IDC_SAVE_PAGE},
    {ui::VKEY_9, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_LAST_TAB},
    {ui::VKEY_NUMPAD9, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_LAST_TAB},
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    {ui::VKEY_9, ui::EF_ALT_DOWN, IDC_SELECT_LAST_TAB},
    {ui::VKEY_NUMPAD9, ui::EF_ALT_DOWN, IDC_SELECT_LAST_TAB},
    {ui::VKEY_NEXT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, IDC_MOVE_TAB_NEXT},
    {ui::VKEY_PRIOR, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     IDC_MOVE_TAB_PREVIOUS},
#endif
    // Control modifier is rarely used on Mac, so we allow it only in several
    // specific cases.
    {ui::VKEY_TAB, ui::EF_CONTROL_DOWN, IDC_SELECT_NEXT_TAB},
    {ui::VKEY_NEXT, ui::EF_CONTROL_DOWN, IDC_SELECT_NEXT_TAB},
    {ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_SELECT_PREVIOUS_TAB},
    {ui::VKEY_PRIOR, ui::EF_CONTROL_DOWN, IDC_SELECT_PREVIOUS_TAB},
    {ui::VKEY_1, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_0},
    {ui::VKEY_NUMPAD1, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_0},
    {ui::VKEY_2, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_1},
    {ui::VKEY_NUMPAD2, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_1},
    {ui::VKEY_3, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_2},
    {ui::VKEY_NUMPAD3, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_2},
    {ui::VKEY_4, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_3},
    {ui::VKEY_NUMPAD4, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_3},
    {ui::VKEY_5, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_4},
    {ui::VKEY_NUMPAD5, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_4},
    {ui::VKEY_6, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_5},
    {ui::VKEY_NUMPAD6, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_5},
    {ui::VKEY_7, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_6},
    {ui::VKEY_NUMPAD7, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_6},
    {ui::VKEY_8, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_7},
    {ui::VKEY_NUMPAD8, ui::EF_PLATFORM_ACCELERATOR, IDC_SELECT_TAB_7},
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    {ui::VKEY_1, ui::EF_ALT_DOWN, IDC_SELECT_TAB_0},
    {ui::VKEY_NUMPAD1, ui::EF_ALT_DOWN, IDC_SELECT_TAB_0},
    {ui::VKEY_2, ui::EF_ALT_DOWN, IDC_SELECT_TAB_1},
    {ui::VKEY_NUMPAD2, ui::EF_ALT_DOWN, IDC_SELECT_TAB_1},
    {ui::VKEY_3, ui::EF_ALT_DOWN, IDC_SELECT_TAB_2},
    {ui::VKEY_NUMPAD3, ui::EF_ALT_DOWN, IDC_SELECT_TAB_2},
    {ui::VKEY_4, ui::EF_ALT_DOWN, IDC_SELECT_TAB_3},
    {ui::VKEY_NUMPAD4, ui::EF_ALT_DOWN, IDC_SELECT_TAB_3},
    {ui::VKEY_5, ui::EF_ALT_DOWN, IDC_SELECT_TAB_4},
    {ui::VKEY_NUMPAD5, ui::EF_ALT_DOWN, IDC_SELECT_TAB_4},
    {ui::VKEY_6, ui::EF_ALT_DOWN, IDC_SELECT_TAB_5},
    {ui::VKEY_NUMPAD6, ui::EF_ALT_DOWN, IDC_SELECT_TAB_5},
    {ui::VKEY_7, ui::EF_ALT_DOWN, IDC_SELECT_TAB_6},
    {ui::VKEY_NUMPAD7, ui::EF_ALT_DOWN, IDC_SELECT_TAB_6},
    {ui::VKEY_8, ui::EF_ALT_DOWN, IDC_SELECT_TAB_7},
    {ui::VKEY_NUMPAD8, ui::EF_ALT_DOWN, IDC_SELECT_TAB_7},
    {ui::VKEY_BROWSER_FAVORITES, ui::EF_NONE, IDC_SHOW_BOOKMARK_BAR},
#endif  // OS_LINUX && !OS_CHROMEOS
    {ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_SHOW_BOOKMARK_BAR},
    {ui::VKEY_ESCAPE, ui::EF_NONE, IDC_STOP},
    {ui::VKEY_OEM_MINUS, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_MINUS},
    {ui::VKEY_SUBTRACT, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_MINUS},
    {ui::VKEY_0, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_NORMAL},
    {ui::VKEY_NUMPAD0, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_NORMAL},
    {ui::VKEY_OEM_PLUS, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_PLUS},
    {ui::VKEY_ADD, ui::EF_PLATFORM_ACCELERATOR, IDC_ZOOM_PLUS},

#if !defined(OS_MACOSX)  // Function keys aren't mapped on Mac.
    {ui::VKEY_F1, ui::EF_NONE, IDC_HELP_PAGE_VIA_KEYBOARD},
    {ui::VKEY_F3, ui::EF_NONE, IDC_FIND_NEXT},
    {ui::VKEY_F3, ui::EF_SHIFT_DOWN, IDC_FIND_PREVIOUS},
    {ui::VKEY_F4, ui::EF_CONTROL_DOWN, IDC_CLOSE_TAB},
    {ui::VKEY_F4, ui::EF_ALT_DOWN, IDC_CLOSE_WINDOW},
    {ui::VKEY_F5, ui::EF_NONE, IDC_RELOAD},
    {ui::VKEY_F5, ui::EF_CONTROL_DOWN, IDC_RELOAD_BYPASSING_CACHE},
    {ui::VKEY_F5, ui::EF_SHIFT_DOWN, IDC_RELOAD_BYPASSING_CACHE},
    {ui::VKEY_F6, ui::EF_NONE, IDC_FOCUS_NEXT_PANE},
    {ui::VKEY_F6, ui::EF_SHIFT_DOWN, IDC_FOCUS_PREVIOUS_PANE},
    {ui::VKEY_F10, ui::EF_NONE, IDC_FOCUS_MENU_BAR},
    {ui::VKEY_F11, ui::EF_NONE, IDC_FULLSCREEN},
#endif  // !OS_MACOSX

  // Platform-specific key maps.
#if defined(OS_LINUX)
    {ui::VKEY_BROWSER_BACK, ui::EF_NONE, IDC_BACK},
    {ui::VKEY_BROWSER_FORWARD, ui::EF_NONE, IDC_FORWARD},
    {ui::VKEY_BROWSER_HOME, ui::EF_NONE, IDC_HOME},
    {ui::VKEY_BROWSER_REFRESH, ui::EF_NONE, IDC_RELOAD},
    {ui::VKEY_BROWSER_REFRESH, ui::EF_CONTROL_DOWN, IDC_RELOAD_BYPASSING_CACHE},
    {ui::VKEY_BROWSER_REFRESH, ui::EF_SHIFT_DOWN, IDC_RELOAD_BYPASSING_CACHE},
#endif  // defined(OS_LINUX)

#if defined(OS_CHROMEOS)
    // On Chrome OS, VKEY_BROWSER_SEARCH is handled in Ash.
    {ui::VKEY_BACK, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_CLEAR_BROWSING_DATA},
    {ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN, IDC_HELP_PAGE_VIA_KEYBOARD},
    {ui::VKEY_OEM_2, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_HELP_PAGE_VIA_KEYBOARD},
    {ui::VKEY_BROWSER_FAVORITES, ui::EF_NONE, IDC_SHOW_BOOKMARK_MANAGER},
    {ui::VKEY_BROWSER_STOP, ui::EF_NONE, IDC_STOP},
    // On Chrome OS, Search + Esc is used to call out task manager.
    {ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN, IDC_TASK_MANAGER},
#else  // !OS_CHROMEOS
    {ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN, IDC_TASK_MANAGER},
    {ui::VKEY_LMENU, ui::EF_NONE, IDC_FOCUS_MENU_BAR},
    {ui::VKEY_MENU, ui::EF_NONE, IDC_FOCUS_MENU_BAR},
    {ui::VKEY_RMENU, ui::EF_NONE, IDC_FOCUS_MENU_BAR},
    // On Windows, all VKEY_BROWSER_* keys except VKEY_BROWSER_SEARCH are
    // handled via WM_APPCOMMAND.
    {ui::VKEY_BROWSER_SEARCH, ui::EF_NONE, IDC_FOCUS_SEARCH},
    {ui::VKEY_M, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_SHOW_AVATAR_MENU},
#if !defined(OS_MACOSX)
    {ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_EXIT},
#endif  // !OS_MACOSX
#endif  // !OS_CHROMEOS

#if defined(GOOGLE_CHROME_BUILD) && !defined(OS_MACOSX)
    {ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, IDC_FEEDBACK},
#endif  // GOOGLE_CHROME_BUILD && !OS_MACOSX
    {ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_NEW_INCOGNITO_WINDOW},
    {ui::VKEY_T, ui::EF_PLATFORM_ACCELERATOR, IDC_NEW_TAB},
    {ui::VKEY_N, ui::EF_PLATFORM_ACCELERATOR, IDC_NEW_WINDOW},
    {ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR,
     IDC_RESTORE_TAB},

#if defined(OS_MACOSX)
    // VKEY_OEM_4 is Left Brace '[{' key.
    {ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN, IDC_BACK},
    {ui::VKEY_LEFT, ui::EF_COMMAND_DOWN, IDC_BACK},
#if BUILDFLAG(ENABLE_PRINTING)
    {ui::VKEY_P, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, IDC_BASIC_PRINT},
#endif  // ENABLE_PRINTING
    {ui::VKEY_BACK, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     IDC_CLEAR_BROWSING_DATA},
    {ui::VKEY_V, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE},
    {ui::VKEY_Z, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     IDC_CONTENT_CONTEXT_REDO},
    {ui::VKEY_A, ui::EF_COMMAND_DOWN, IDC_CONTENT_CONTEXT_SELECTALL},
    {ui::VKEY_Z, ui::EF_COMMAND_DOWN, IDC_CONTENT_CONTEXT_UNDO},
    {ui::VKEY_I, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, IDC_DEV_TOOLS},
    {ui::VKEY_J, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, IDC_DEV_TOOLS_CONSOLE},
    {ui::VKEY_C, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     IDC_DEV_TOOLS_INSPECT},
    {ui::VKEY_I, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     IDC_EMAIL_PAGE_LOCATION},
    {ui::VKEY_Q, ui::EF_COMMAND_DOWN, IDC_EXIT},
    {ui::VKEY_F, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, IDC_FOCUS_SEARCH},
    // VKEY_OEM_6 is Right Brace ']}' key.
    {ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN, IDC_FORWARD},
    {ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN, IDC_FORWARD},
    {ui::VKEY_F, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN, IDC_FULLSCREEN},
    // VKEY_OEM_2 is Slash '/?' key.
    {ui::VKEY_OEM_2, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     IDC_HELP_PAGE_VIA_MENU},
    {ui::VKEY_H, ui::EF_COMMAND_DOWN, IDC_HIDE_APP},
    {ui::VKEY_H, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN, IDC_HOME},
    {ui::VKEY_M, ui::EF_COMMAND_DOWN, IDC_MINIMIZE_WINDOW},
    {ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
     IDC_SELECT_NEXT_TAB},
    {ui::VKEY_LEFT, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
     IDC_SELECT_PREVIOUS_TAB},
    {ui::VKEY_B, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
     IDC_SHOW_BOOKMARK_MANAGER},
    {ui::VKEY_J, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN, IDC_SHOW_DOWNLOADS},
    {ui::VKEY_L, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, IDC_SHOW_DOWNLOADS},
    {ui::VKEY_Y, ui::EF_COMMAND_DOWN, IDC_SHOW_HISTORY},
    {ui::VKEY_OEM_PERIOD, ui::EF_COMMAND_DOWN, IDC_STOP},
    {ui::VKEY_U, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, IDC_VIEW_SOURCE},
#else  // !OS_MACOSX
    // Alt by itself (or with just shift) is never used on Mac since it's used
    // to generate non-ASCII characters. Such commands are given Mac-specific
    // bindings as well. Mapping with just Alt appear here, and should have an
    // alternative mapping in the block above.
    {ui::VKEY_LEFT, ui::EF_ALT_DOWN, IDC_BACK},
#if BUILDFLAG(ENABLE_PRINTING)
    {ui::VKEY_P, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_BASIC_PRINT},
#endif  // ENABLE_PRINTING
#if !defined(OS_CHROMEOS)
    {ui::VKEY_DELETE, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_CLEAR_BROWSING_DATA},
#endif  // !OS_CHROMEOS
    {ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_DEV_TOOLS},
    {ui::VKEY_J, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_DEV_TOOLS_CONSOLE},
    {ui::VKEY_C, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_DEV_TOOLS_INSPECT},
    {ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, IDC_FOCUS_BOOKMARKS},
    {ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     IDC_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY},
    {ui::VKEY_D, ui::EF_ALT_DOWN, IDC_FOCUS_LOCATION},
    {ui::VKEY_E, ui::EF_CONTROL_DOWN, IDC_FOCUS_SEARCH},
    {ui::VKEY_K, ui::EF_CONTROL_DOWN, IDC_FOCUS_SEARCH},
    {ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, IDC_FOCUS_TOOLBAR},
    {ui::VKEY_RIGHT, ui::EF_ALT_DOWN, IDC_FORWARD},
    {ui::VKEY_HOME, ui::EF_ALT_DOWN, IDC_HOME},
    {ui::VKEY_E, ui::EF_ALT_DOWN, IDC_SHOW_APP_MENU},
    {ui::VKEY_F, ui::EF_ALT_DOWN, IDC_SHOW_APP_MENU},
    {ui::VKEY_O, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_SHOW_BOOKMARK_MANAGER},
    {ui::VKEY_J, ui::EF_CONTROL_DOWN, IDC_SHOW_DOWNLOADS},
    {ui::VKEY_H, ui::EF_CONTROL_DOWN, IDC_SHOW_HISTORY},
    {ui::VKEY_U, ui::EF_CONTROL_DOWN, IDC_VIEW_SOURCE},
#if !defined(OS_CHROMEOS)
    // On Chrome OS, these keys are assigned to change UI scale.
    {ui::VKEY_OEM_MINUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_ZOOM_MINUS},
    {ui::VKEY_OEM_PLUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS},
#endif  // !OS_CHROMEOS
#endif  // OS_MACOSX
};

const int kRepeatableCommandIds[] = {
  IDC_FIND_NEXT,
  IDC_FIND_PREVIOUS,
  IDC_FOCUS_NEXT_PANE,
  IDC_FOCUS_PREVIOUS_PANE,
  IDC_MOVE_TAB_NEXT,
  IDC_MOVE_TAB_PREVIOUS,
  IDC_SELECT_NEXT_TAB,
  IDC_SELECT_PREVIOUS_TAB,
};
const size_t kRepeatableCommandIdsLength = arraysize(kRepeatableCommandIds);

} // namespace

std::vector<AcceleratorMapping> GetAcceleratorList() {
  CR_DEFINE_STATIC_LOCAL(
      std::vector<AcceleratorMapping>, accelerators,
      (std::begin(kAcceleratorMap), std::end(kAcceleratorMap)));
  return accelerators;
}

bool GetStandardAcceleratorForCommandId(int command_id,
                                        ui::Accelerator* accelerator) {
  // The standard Ctrl-X, Ctrl-V and Ctrl-C are not defined as accelerators
  // anywhere else.
  switch (command_id) {
    case IDC_CUT:
      *accelerator = ui::Accelerator(ui::VKEY_X, ui::EF_PLATFORM_ACCELERATOR);
      return true;
    case IDC_COPY:
      *accelerator = ui::Accelerator(ui::VKEY_C, ui::EF_PLATFORM_ACCELERATOR);
      return true;
    case IDC_PASTE:
      *accelerator = ui::Accelerator(ui::VKEY_V, ui::EF_PLATFORM_ACCELERATOR);
      return true;
  }
  return false;
}

bool IsCommandRepeatable(int command_id) {
  for (size_t i = 0; i < kRepeatableCommandIdsLength; ++i) {
    if (kRepeatableCommandIds[i] == command_id)
      return true;
  }
  return false;
}
