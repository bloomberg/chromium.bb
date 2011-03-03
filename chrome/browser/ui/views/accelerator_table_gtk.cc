// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accelerator_table_gtk.h"

#include "base/basictypes.h"
#include "chrome/app/chrome_command_ids.h"
#include "ui/base/keycodes/keyboard_codes.h"

namespace browser {

// NOTE: Keep this list in the same (mostly-alphabetical) order as
// the Windows accelerators in ../../app/chrome_dll.rc.
const AcceleratorMapping kAcceleratorMap[] = {
  // Keycode                  Shift  Ctrl   Alt    Command ID
  { ui::VKEY_A,              true,  true,  false, IDC_AUTOFILL_DEFAULT },
  { ui::VKEY_LEFT,           false, false, true,  IDC_BACK },
  { ui::VKEY_BACK,           false, false, false, IDC_BACK },
#if defined(OS_CHROMEOS)
  { ui::VKEY_F1,             false, false, false, IDC_BACK },
  { ui::VKEY_OEM_2,          false, true,  true,  IDC_SHOW_KEYBOARD_OVERLAY },
  { ui::VKEY_OEM_2,          true,  true,  true,  IDC_SHOW_KEYBOARD_OVERLAY },
#endif
  { ui::VKEY_D,              false, true,  false, IDC_BOOKMARK_PAGE },
  { ui::VKEY_D,              true,  true,  false, IDC_BOOKMARK_ALL_TABS },
#if !defined(OS_CHROMEOS)
  { ui::VKEY_DELETE,         true,  true,  false, IDC_CLEAR_BROWSING_DATA },
#else
  { ui::VKEY_BACK,           true,  true,  false, IDC_CLEAR_BROWSING_DATA },
#endif
#if !defined(OS_CHROMEOS)
  { ui::VKEY_F4,             false, true,  false, IDC_CLOSE_TAB },
#endif
  { ui::VKEY_W,              false, true,  false, IDC_CLOSE_TAB },
  { ui::VKEY_W,              true,  true,  false, IDC_CLOSE_WINDOW },
#if !defined(OS_CHROMEOS)
  { ui::VKEY_F4,             false, false, true,  IDC_CLOSE_WINDOW },
#endif
  { ui::VKEY_Q,              true,  true,  false, IDC_EXIT },
  { ui::VKEY_F,              false, true,  false, IDC_FIND },
  { ui::VKEY_G,              false, true,  false, IDC_FIND_NEXT },
#if !defined(OS_CHROMEOS)
  { ui::VKEY_F3,             false, false, false, IDC_FIND_NEXT },
#endif
  { ui::VKEY_G,              true,  true,  false, IDC_FIND_PREVIOUS },
#if !defined(OS_CHROMEOS)
  { ui::VKEY_F3,             true,  false, false, IDC_FIND_PREVIOUS },
#endif
#if defined(OS_CHROMEOS)
  { ui::VKEY_S,              true,  false, true,  IDC_FOCUS_CHROMEOS_STATUS },
#endif
  { ui::VKEY_D,              false, false, true,  IDC_FOCUS_LOCATION },
  { ui::VKEY_L,              false, true,  false, IDC_FOCUS_LOCATION },
#if !defined(OS_CHROMEOS)
  { ui::VKEY_F10,            false, false, false, IDC_FOCUS_MENU_BAR },
#endif
  { ui::VKEY_MENU,           false, false, false, IDC_FOCUS_MENU_BAR },
#if !defined(OS_CHROMEOS)
  { ui::VKEY_F6,             false, false, false, IDC_FOCUS_NEXT_PANE },
#else
  { ui::VKEY_F2,             false, true,  false, IDC_FOCUS_NEXT_PANE },
#endif
#if !defined(OS_CHROMEOS)
  { ui::VKEY_F6,             true,  false, false, IDC_FOCUS_PREVIOUS_PANE },
#else
  { ui::VKEY_F1,             false, true,  false, IDC_FOCUS_PREVIOUS_PANE },
#endif
  { ui::VKEY_K,              false, true,  false, IDC_FOCUS_SEARCH },
  { ui::VKEY_E,              false, true,  false, IDC_FOCUS_SEARCH },
  { ui::VKEY_BROWSER_SEARCH, false, false, false, IDC_FOCUS_SEARCH },
  { ui::VKEY_T,              true,  false, true,  IDC_FOCUS_TOOLBAR },
  { ui::VKEY_B,              true,  false, true,  IDC_FOCUS_BOOKMARKS },
  { ui::VKEY_RIGHT,          false, false, true,  IDC_FORWARD },
  { ui::VKEY_BACK,           true,  false, false, IDC_FORWARD },
#if defined(OS_CHROMEOS)
  { ui::VKEY_F2,             false, false, false, IDC_FORWARD },
#endif
#if !defined(OS_CHROMEOS)
  { ui::VKEY_F11,            false, false, false, IDC_FULLSCREEN },
#else
  { ui::VKEY_F4,             false, false, false, IDC_FULLSCREEN },
#endif
#if !defined(OS_CHROMEOS)
  { ui::VKEY_F1,             false, false, false, IDC_HELP_PAGE },
#else
  { ui::VKEY_OEM_2,          false, true,  false, IDC_HELP_PAGE },
  { ui::VKEY_OEM_2,          true,  true,  false, IDC_HELP_PAGE },
#endif
  { ui::VKEY_I,              true,  true,  false, IDC_DEV_TOOLS },
  { ui::VKEY_J,              true,  true,  false, IDC_DEV_TOOLS_CONSOLE },
  { ui::VKEY_C,              true,  true,  false, IDC_DEV_TOOLS_INSPECT },
  { ui::VKEY_N,              true,  true,  false, IDC_NEW_INCOGNITO_WINDOW },
  { ui::VKEY_T,              false, true,  false, IDC_NEW_TAB },
  { ui::VKEY_N,              false, true,  false, IDC_NEW_WINDOW },
  { ui::VKEY_O,              false, true,  false, IDC_OPEN_FILE },
  { ui::VKEY_P,              false, true,  false, IDC_PRINT},
  { ui::VKEY_R,              false, true,  false, IDC_RELOAD },
  { ui::VKEY_R,              true,  true,  false, IDC_RELOAD_IGNORING_CACHE },
#if !defined(OS_CHROMEOS)
  { ui::VKEY_F5,             false, false, false, IDC_RELOAD },
  { ui::VKEY_F5,             false, true,  false, IDC_RELOAD_IGNORING_CACHE },
  { ui::VKEY_F5,             true,  false, false, IDC_RELOAD_IGNORING_CACHE },
#else
  { ui::VKEY_F3,             false, false, false, IDC_RELOAD },
  { ui::VKEY_F3,             false, true,  false, IDC_RELOAD_IGNORING_CACHE },
  { ui::VKEY_F3,             true,  false, false, IDC_RELOAD_IGNORING_CACHE },
#endif
  { ui::VKEY_HOME,           false, false, true,  IDC_HOME },
  { ui::VKEY_T,              true,  true,  false, IDC_RESTORE_TAB },
  { ui::VKEY_S,              false, true,  false, IDC_SAVE_PAGE },
#if defined(OS_CHROMEOS)
  { ui::VKEY_LWIN,           false, false, false, IDC_SEARCH },
#endif
  { ui::VKEY_9,              false, true,  false, IDC_SELECT_LAST_TAB },
  { ui::VKEY_NUMPAD9,        false, true,  false, IDC_SELECT_LAST_TAB },
  { ui::VKEY_TAB,            false, true,  false, IDC_SELECT_NEXT_TAB },
  { ui::VKEY_NEXT,           false, true,  false, IDC_SELECT_NEXT_TAB },
  { ui::VKEY_TAB,            true,  true,  false, IDC_SELECT_PREVIOUS_TAB },
  { ui::VKEY_PRIOR,          false, true,  false, IDC_SELECT_PREVIOUS_TAB },
  { ui::VKEY_1,              false, true,  false, IDC_SELECT_TAB_0 },
  { ui::VKEY_NUMPAD1,        false, true,  false, IDC_SELECT_TAB_0 },
  { ui::VKEY_2,              false, true,  false, IDC_SELECT_TAB_1 },
  { ui::VKEY_NUMPAD2,        false, true,  false, IDC_SELECT_TAB_1 },
  { ui::VKEY_3,              false, true,  false, IDC_SELECT_TAB_2 },
  { ui::VKEY_NUMPAD3,        false, true,  false, IDC_SELECT_TAB_2 },
  { ui::VKEY_4,              false, true,  false, IDC_SELECT_TAB_3 },
  { ui::VKEY_NUMPAD4,        false, true,  false, IDC_SELECT_TAB_3 },
  { ui::VKEY_5,              false, true,  false, IDC_SELECT_TAB_4 },
  { ui::VKEY_NUMPAD5,        false, true,  false, IDC_SELECT_TAB_4 },
  { ui::VKEY_6,              false, true,  false, IDC_SELECT_TAB_5 },
  { ui::VKEY_NUMPAD6,        false, true,  false, IDC_SELECT_TAB_5 },
  { ui::VKEY_7,              false, true,  false, IDC_SELECT_TAB_6 },
  { ui::VKEY_NUMPAD7,        false, true,  false, IDC_SELECT_TAB_6 },
  { ui::VKEY_8,              false, true,  false, IDC_SELECT_TAB_7 },
  { ui::VKEY_NUMPAD8,        false, true,  false, IDC_SELECT_TAB_7 },
  { ui::VKEY_B,              true,  true,  false, IDC_SHOW_BOOKMARK_BAR },
  { ui::VKEY_J,              false, true,  false, IDC_SHOW_DOWNLOADS },
  { ui::VKEY_H,              false, true,  false, IDC_SHOW_HISTORY },
  { ui::VKEY_F,              false, false, true,  IDC_SHOW_APP_MENU},
  { ui::VKEY_E,              false, false, true,  IDC_SHOW_APP_MENU},
  { ui::VKEY_ESCAPE,         false, false, false, IDC_STOP },
  { ui::VKEY_ESCAPE,         true,  false, false, IDC_TASK_MANAGER },
  { ui::VKEY_U,              false, true,  false, IDC_VIEW_SOURCE },
  { ui::VKEY_OEM_MINUS,      false, true,  false, IDC_ZOOM_MINUS },
  { ui::VKEY_OEM_MINUS,      true,  true,  false, IDC_ZOOM_MINUS },
  { ui::VKEY_SUBTRACT,       false, true,  false, IDC_ZOOM_MINUS },
  { ui::VKEY_0,              false, true,  false, IDC_ZOOM_NORMAL },
  { ui::VKEY_NUMPAD0,        false, true,  false, IDC_ZOOM_NORMAL },
  { ui::VKEY_OEM_PLUS,       false, true,  false, IDC_ZOOM_PLUS },
  { ui::VKEY_OEM_PLUS,       true,  true,  false, IDC_ZOOM_PLUS },
  { ui::VKEY_ADD,            false, true,  false, IDC_ZOOM_PLUS },
};

const size_t kAcceleratorMapLength = arraysize(kAcceleratorMap);

}  // namespace browser
