// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/accelerator_table_gtk.h"

#include "base/basictypes.h"
#include "base/keyboard_codes.h"
#include "chrome/app/chrome_dll_resource.h"

namespace browser {

// NOTE: Keep this list in the same (mostly-alphabetical) order as
// the Windows accelerators in ../../app/chrome_dll.rc.
const AcceleratorMapping kAcceleratorMap[] = {
  // Keycode                   Shift  Ctrl   Alt    Command ID
  { base::VKEY_A,              true,  true,  false, IDC_AUTOFILL_DEFAULT },
  { base::VKEY_LEFT,           false, false, true,  IDC_BACK },
  { base::VKEY_BACK,           false, false, false, IDC_BACK },
#if defined(OS_CHROMEOS)
  { base::VKEY_F1,             false, false, false, IDC_BACK },
#endif
  { base::VKEY_D,              false, true,  false, IDC_BOOKMARK_PAGE },
  { base::VKEY_D,              true,  true,  false, IDC_BOOKMARK_ALL_TABS },
  { base::VKEY_DELETE,         true,  true,  false, IDC_CLEAR_BROWSING_DATA },
#if !defined(OS_CHROMEOS)
  { base::VKEY_F4,             false, true,  false, IDC_CLOSE_TAB },
#endif
  { base::VKEY_W,              false, true,  false, IDC_CLOSE_TAB },
  { base::VKEY_W,              true,  true,  false, IDC_CLOSE_WINDOW },
#if !defined(OS_CHROMEOS)
  { base::VKEY_F4,             false, false, true,  IDC_CLOSE_WINDOW },
#endif
  { base::VKEY_Q,              true,  true,  false, IDC_EXIT },
  { base::VKEY_F,              false, true,  false, IDC_FIND },
  { base::VKEY_G,              false, true,  false, IDC_FIND_NEXT },
#if !defined(OS_CHROMEOS)
  { base::VKEY_F3,             false, false, false, IDC_FIND_NEXT },
#endif
  { base::VKEY_G,              true,  true,  false, IDC_FIND_PREVIOUS },
#if !defined(OS_CHROMEOS)
  { base::VKEY_F3,             true,  false, false, IDC_FIND_PREVIOUS },
#endif
#if defined(OS_CHROMEOS)
  { base::VKEY_S,              true,  false, true,  IDC_FOCUS_CHROMEOS_STATUS },
#endif
  { base::VKEY_D,              false, false, true,  IDC_FOCUS_LOCATION },
  { base::VKEY_L,              false, true,  false, IDC_FOCUS_LOCATION },
#if !defined(OS_CHROMEOS)
  { base::VKEY_F10,            false, false, false, IDC_FOCUS_MENU_BAR },
#endif
  { base::VKEY_MENU,           false, false, false, IDC_FOCUS_MENU_BAR },
#if !defined(OS_CHROMEOS)
  { base::VKEY_F6,             false, false, false, IDC_FOCUS_NEXT_PANE },
#endif
#if defined(OS_CHROMEOS)
  { base::VKEY_F2,             false, true,  false, IDC_FOCUS_NEXT_PANE },
#endif
#if !defined(OS_CHROMEOS)
  { base::VKEY_F6,             true,  false, false, IDC_FOCUS_PREVIOUS_PANE },
#endif
#if defined(OS_CHROMEOS)
  { base::VKEY_F1,             false, true,  false, IDC_FOCUS_PREVIOUS_PANE },
#endif
  { base::VKEY_K,              false, true,  false, IDC_FOCUS_SEARCH },
  { base::VKEY_E,              false, true,  false, IDC_FOCUS_SEARCH },
  { base::VKEY_BROWSER_SEARCH, false, false, false, IDC_FOCUS_SEARCH },
  { base::VKEY_T,              true,  false, true,  IDC_FOCUS_TOOLBAR },
  { base::VKEY_B,              true,  false, true,  IDC_FOCUS_BOOKMARKS },
  { base::VKEY_RIGHT,          false, false, true,  IDC_FORWARD },
  { base::VKEY_BACK,           true,  false, false, IDC_FORWARD },
#if defined(OS_CHROMEOS)
  { base::VKEY_F2,             false, false, false, IDC_FORWARD },
#endif
#if !defined(OS_CHROMEOS)
  { base::VKEY_F11,            false, false, false, IDC_FULLSCREEN },
#endif
#if defined(OS_CHROMEOS)
  { base::VKEY_F4,             false, false, false, IDC_FULLSCREEN },
#endif
#if !defined(OS_CHROMEOS)
  { base::VKEY_F1,             false, false, false, IDC_HELP_PAGE },
#endif
  { base::VKEY_I,              true,  true,  false, IDC_DEV_TOOLS },
  { base::VKEY_J,              true,  true,  false, IDC_DEV_TOOLS_CONSOLE },
  { base::VKEY_C,              true,  true,  false, IDC_DEV_TOOLS_INSPECT },
  { base::VKEY_N,              true,  true,  false, IDC_NEW_INCOGNITO_WINDOW },
  { base::VKEY_T,              false, true,  false, IDC_NEW_TAB },
  { base::VKEY_N,              false, true,  false, IDC_NEW_WINDOW },
  { base::VKEY_O,              false, true,  false, IDC_OPEN_FILE },
  { base::VKEY_P,              false, true,  false, IDC_PRINT},
  { base::VKEY_R,              false, true,  false, IDC_RELOAD },
  { base::VKEY_R,              true,  true,  false, IDC_RELOAD_IGNORING_CACHE },
#if !defined(OS_CHROMEOS)
  { base::VKEY_F5,             false, false, false, IDC_RELOAD },
  { base::VKEY_F5,             false, true,  false, IDC_RELOAD_IGNORING_CACHE },
  { base::VKEY_F5,             true,  false, false, IDC_RELOAD_IGNORING_CACHE },
#endif
  { base::VKEY_HOME,           false, false, true,  IDC_HOME },
  { base::VKEY_T,              true,  true,  false, IDC_RESTORE_TAB },
  { base::VKEY_S,              false, true,  false, IDC_SAVE_PAGE },
#if defined(OS_CHROMEOS)
  { base::VKEY_LWIN,           false, false, false, IDC_SEARCH },
#endif
  { base::VKEY_9,              false, true,  false, IDC_SELECT_LAST_TAB },
  { base::VKEY_NUMPAD9,        false, true,  false, IDC_SELECT_LAST_TAB },
  { base::VKEY_TAB,            false, true,  false, IDC_SELECT_NEXT_TAB },
  { base::VKEY_NEXT,           false, true,  false, IDC_SELECT_NEXT_TAB },
  { base::VKEY_TAB,            true,  true,  false, IDC_SELECT_PREVIOUS_TAB },
  { base::VKEY_PRIOR,          false, true,  false, IDC_SELECT_PREVIOUS_TAB },
  { base::VKEY_1,              false, true,  false, IDC_SELECT_TAB_0 },
  { base::VKEY_NUMPAD1,        false, true,  false, IDC_SELECT_TAB_0 },
  { base::VKEY_2,              false, true,  false, IDC_SELECT_TAB_1 },
  { base::VKEY_NUMPAD2,        false, true,  false, IDC_SELECT_TAB_1 },
  { base::VKEY_3,              false, true,  false, IDC_SELECT_TAB_2 },
  { base::VKEY_NUMPAD3,        false, true,  false, IDC_SELECT_TAB_2 },
  { base::VKEY_4,              false, true,  false, IDC_SELECT_TAB_3 },
  { base::VKEY_NUMPAD4,        false, true,  false, IDC_SELECT_TAB_3 },
  { base::VKEY_5,              false, true,  false, IDC_SELECT_TAB_4 },
  { base::VKEY_NUMPAD5,        false, true,  false, IDC_SELECT_TAB_4 },
  { base::VKEY_6,              false, true,  false, IDC_SELECT_TAB_5 },
  { base::VKEY_NUMPAD6,        false, true,  false, IDC_SELECT_TAB_5 },
  { base::VKEY_7,              false, true,  false, IDC_SELECT_TAB_6 },
  { base::VKEY_NUMPAD7,        false, true,  false, IDC_SELECT_TAB_6 },
  { base::VKEY_8,              false, true,  false, IDC_SELECT_TAB_7 },
  { base::VKEY_NUMPAD8,        false, true,  false, IDC_SELECT_TAB_7 },
  { base::VKEY_B,              true,  true,  false, IDC_SHOW_BOOKMARK_BAR },
  { base::VKEY_J,              false, true,  false, IDC_SHOW_DOWNLOADS },
  { base::VKEY_H,              false, true,  false, IDC_SHOW_HISTORY },
  { base::VKEY_F,              false, false, true,  IDC_SHOW_APP_MENU},
  { base::VKEY_E,              false, false, true,  IDC_SHOW_APP_MENU},
  { base::VKEY_ESCAPE,         false, false, false, IDC_STOP },
#if defined(OS_CHROMEOS)
  { base::VKEY_F5,             false, false, false, IDC_SYSTEM_OPTIONS },
#endif
  { base::VKEY_ESCAPE,         true,  false, false, IDC_TASK_MANAGER },
  { base::VKEY_U,              false, true,  false, IDC_VIEW_SOURCE },
  { base::VKEY_OEM_MINUS,      false, true,  false, IDC_ZOOM_MINUS },
  { base::VKEY_OEM_MINUS,      true,  true,  false, IDC_ZOOM_MINUS },
  { base::VKEY_SUBTRACT,       false, true,  false, IDC_ZOOM_MINUS },
  { base::VKEY_0,              false, true,  false, IDC_ZOOM_NORMAL },
  { base::VKEY_NUMPAD0,        false, true,  false, IDC_ZOOM_NORMAL },
  { base::VKEY_OEM_PLUS,       false, true,  false, IDC_ZOOM_PLUS },
  { base::VKEY_OEM_PLUS,       true,  true,  false, IDC_ZOOM_PLUS },
  { base::VKEY_ADD,            false, true,  false, IDC_ZOOM_PLUS },
};

const size_t kAcceleratorMapLength = arraysize(kAcceleratorMap);

}  // namespace browser
