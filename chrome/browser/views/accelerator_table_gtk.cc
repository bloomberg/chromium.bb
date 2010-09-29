// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/accelerator_table_gtk.h"

#include "app/keyboard_codes.h"
#include "base/basictypes.h"
#include "chrome/app/chrome_dll_resource.h"

namespace browser {

// NOTE: Keep this list in the same (mostly-alphabetical) order as
// the Windows accelerators in ../../app/chrome_dll.rc.
const AcceleratorMapping kAcceleratorMap[] = {
  // Keycode                   Shift  Ctrl   Alt    Command ID
  { app::VKEY_A,              true,  true,  false, IDC_AUTOFILL_DEFAULT },
  { app::VKEY_LEFT,           false, false, true,  IDC_BACK },
  { app::VKEY_BACK,           false, false, false, IDC_BACK },
#if defined(OS_CHROMEOS)
  { app::VKEY_F1,             false, false, false, IDC_BACK },
#endif
  { app::VKEY_D,              false, true,  false, IDC_BOOKMARK_PAGE },
  { app::VKEY_D,              true,  true,  false, IDC_BOOKMARK_ALL_TABS },
  { app::VKEY_DELETE,         true,  true,  false, IDC_CLEAR_BROWSING_DATA },
#if !defined(OS_CHROMEOS)
  { app::VKEY_F4,             false, true,  false, IDC_CLOSE_TAB },
#endif
  { app::VKEY_W,              false, true,  false, IDC_CLOSE_TAB },
  { app::VKEY_W,              true,  true,  false, IDC_CLOSE_WINDOW },
#if !defined(OS_CHROMEOS)
  { app::VKEY_F4,             false, false, true,  IDC_CLOSE_WINDOW },
#endif
  { app::VKEY_Q,              true,  true,  false, IDC_EXIT },
  { app::VKEY_F,              false, true,  false, IDC_FIND },
  { app::VKEY_G,              false, true,  false, IDC_FIND_NEXT },
#if !defined(OS_CHROMEOS)
  { app::VKEY_F3,             false, false, false, IDC_FIND_NEXT },
#endif
  { app::VKEY_G,              true,  true,  false, IDC_FIND_PREVIOUS },
#if !defined(OS_CHROMEOS)
  { app::VKEY_F3,             true,  false, false, IDC_FIND_PREVIOUS },
#endif
#if defined(OS_CHROMEOS)
  { app::VKEY_S,              true,  false, true,  IDC_FOCUS_CHROMEOS_STATUS },
#endif
  { app::VKEY_D,              false, false, true,  IDC_FOCUS_LOCATION },
  { app::VKEY_L,              false, true,  false, IDC_FOCUS_LOCATION },
#if !defined(OS_CHROMEOS)
  { app::VKEY_F10,            false, false, false, IDC_FOCUS_MENU_BAR },
#endif
  { app::VKEY_MENU,           false, false, false, IDC_FOCUS_MENU_BAR },
#if !defined(OS_CHROMEOS)
  { app::VKEY_F6,             false, false, false, IDC_FOCUS_NEXT_PANE },
#endif
#if defined(OS_CHROMEOS)
  { app::VKEY_F2,             false, true,  false, IDC_FOCUS_NEXT_PANE },
#endif
#if !defined(OS_CHROMEOS)
  { app::VKEY_F6,             true,  false, false, IDC_FOCUS_PREVIOUS_PANE },
#endif
#if defined(OS_CHROMEOS)
  { app::VKEY_F1,             false, true,  false, IDC_FOCUS_PREVIOUS_PANE },
#endif
  { app::VKEY_K,              false, true,  false, IDC_FOCUS_SEARCH },
  { app::VKEY_E,              false, true,  false, IDC_FOCUS_SEARCH },
  { app::VKEY_BROWSER_SEARCH, false, false, false, IDC_FOCUS_SEARCH },
  { app::VKEY_T,              true,  false, true,  IDC_FOCUS_TOOLBAR },
  { app::VKEY_B,              true,  false, true,  IDC_FOCUS_BOOKMARKS },
  { app::VKEY_RIGHT,          false, false, true,  IDC_FORWARD },
  { app::VKEY_BACK,           true,  false, false, IDC_FORWARD },
#if defined(OS_CHROMEOS)
  { app::VKEY_F2,             false, false, false, IDC_FORWARD },
#endif
#if !defined(OS_CHROMEOS)
  { app::VKEY_F11,            false, false, false, IDC_FULLSCREEN },
#endif
#if defined(OS_CHROMEOS)
  { app::VKEY_F4,             false, false, false, IDC_FULLSCREEN },
#endif
#if !defined(OS_CHROMEOS)
  { app::VKEY_F1,             false, false, false, IDC_HELP_PAGE },
#endif
  { app::VKEY_I,              true,  true,  false, IDC_DEV_TOOLS },
  { app::VKEY_J,              true,  true,  false, IDC_DEV_TOOLS_CONSOLE },
  { app::VKEY_C,              true,  true,  false, IDC_DEV_TOOLS_INSPECT },
  { app::VKEY_N,              true,  true,  false, IDC_NEW_INCOGNITO_WINDOW },
  { app::VKEY_T,              false, true,  false, IDC_NEW_TAB },
  { app::VKEY_N,              false, true,  false, IDC_NEW_WINDOW },
  { app::VKEY_O,              false, true,  false, IDC_OPEN_FILE },
  { app::VKEY_P,              false, true,  false, IDC_PRINT},
  { app::VKEY_R,              false, true,  false, IDC_RELOAD },
  { app::VKEY_R,              true,  true,  false, IDC_RELOAD_IGNORING_CACHE },
#if !defined(OS_CHROMEOS)
  { app::VKEY_F5,             false, false, false, IDC_RELOAD },
  { app::VKEY_F5,             false, true,  false, IDC_RELOAD_IGNORING_CACHE },
  { app::VKEY_F5,             true,  false, false, IDC_RELOAD_IGNORING_CACHE },
#endif
#if defined(OS_CHROMEOS)
  { app::VKEY_F3,             false, false, false, IDC_RELOAD },
  { app::VKEY_F3,             false, true,  false, IDC_RELOAD_IGNORING_CACHE },
  { app::VKEY_F3,             true,  false, false, IDC_RELOAD_IGNORING_CACHE },
#endif
  { app::VKEY_HOME,           false, false, true,  IDC_HOME },
  { app::VKEY_T,              true,  true,  false, IDC_RESTORE_TAB },
  { app::VKEY_S,              false, true,  false, IDC_SAVE_PAGE },
#if defined(OS_CHROMEOS)
  { app::VKEY_LWIN,           false, false, false, IDC_SEARCH },
#endif
  { app::VKEY_9,              false, true,  false, IDC_SELECT_LAST_TAB },
  { app::VKEY_NUMPAD9,        false, true,  false, IDC_SELECT_LAST_TAB },
  { app::VKEY_TAB,            false, true,  false, IDC_SELECT_NEXT_TAB },
  { app::VKEY_NEXT,           false, true,  false, IDC_SELECT_NEXT_TAB },
  { app::VKEY_TAB,            true,  true,  false, IDC_SELECT_PREVIOUS_TAB },
  { app::VKEY_PRIOR,          false, true,  false, IDC_SELECT_PREVIOUS_TAB },
  { app::VKEY_1,              false, true,  false, IDC_SELECT_TAB_0 },
  { app::VKEY_NUMPAD1,        false, true,  false, IDC_SELECT_TAB_0 },
  { app::VKEY_2,              false, true,  false, IDC_SELECT_TAB_1 },
  { app::VKEY_NUMPAD2,        false, true,  false, IDC_SELECT_TAB_1 },
  { app::VKEY_3,              false, true,  false, IDC_SELECT_TAB_2 },
  { app::VKEY_NUMPAD3,        false, true,  false, IDC_SELECT_TAB_2 },
  { app::VKEY_4,              false, true,  false, IDC_SELECT_TAB_3 },
  { app::VKEY_NUMPAD4,        false, true,  false, IDC_SELECT_TAB_3 },
  { app::VKEY_5,              false, true,  false, IDC_SELECT_TAB_4 },
  { app::VKEY_NUMPAD5,        false, true,  false, IDC_SELECT_TAB_4 },
  { app::VKEY_6,              false, true,  false, IDC_SELECT_TAB_5 },
  { app::VKEY_NUMPAD6,        false, true,  false, IDC_SELECT_TAB_5 },
  { app::VKEY_7,              false, true,  false, IDC_SELECT_TAB_6 },
  { app::VKEY_NUMPAD7,        false, true,  false, IDC_SELECT_TAB_6 },
  { app::VKEY_8,              false, true,  false, IDC_SELECT_TAB_7 },
  { app::VKEY_NUMPAD8,        false, true,  false, IDC_SELECT_TAB_7 },
  { app::VKEY_B,              true,  true,  false, IDC_SHOW_BOOKMARK_BAR },
  { app::VKEY_J,              false, true,  false, IDC_SHOW_DOWNLOADS },
  { app::VKEY_H,              false, true,  false, IDC_SHOW_HISTORY },
  { app::VKEY_F,              false, false, true,  IDC_SHOW_APP_MENU},
  { app::VKEY_E,              false, false, true,  IDC_SHOW_APP_MENU},
  { app::VKEY_ESCAPE,         false, false, false, IDC_STOP },
#if defined(OS_CHROMEOS)
  { app::VKEY_F5,             false, false, false, IDC_SYSTEM_OPTIONS },
#endif
  { app::VKEY_ESCAPE,         true,  false, false, IDC_TASK_MANAGER },
  { app::VKEY_U,              false, true,  false, IDC_VIEW_SOURCE },
  { app::VKEY_OEM_MINUS,      false, true,  false, IDC_ZOOM_MINUS },
  { app::VKEY_OEM_MINUS,      true,  true,  false, IDC_ZOOM_MINUS },
  { app::VKEY_SUBTRACT,       false, true,  false, IDC_ZOOM_MINUS },
  { app::VKEY_0,              false, true,  false, IDC_ZOOM_NORMAL },
  { app::VKEY_NUMPAD0,        false, true,  false, IDC_ZOOM_NORMAL },
  { app::VKEY_OEM_PLUS,       false, true,  false, IDC_ZOOM_PLUS },
  { app::VKEY_OEM_PLUS,       true,  true,  false, IDC_ZOOM_PLUS },
  { app::VKEY_ADD,            false, true,  false, IDC_ZOOM_PLUS },
};

const size_t kAcceleratorMapLength = arraysize(kAcceleratorMap);

}  // namespace browser
