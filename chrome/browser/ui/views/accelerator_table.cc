// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accelerator_table.h"

#include "base/basictypes.h"
#include "chrome/app/chrome_command_ids.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"

#if defined(USE_ASH)
#include "ash/accelerators/accelerator_table.h"
#endif

namespace chrome {
namespace {

// NOTE: Keep this list in the same (mostly-alphabetical) order as
// the Windows accelerators in ../../app/chrome_dll.rc.
// Do not use Ctrl-Alt as a shortcut modifier, as it is used by i18n keyboards:
// http://blogs.msdn.com/b/oldnewthing/archive/2004/03/29/101121.aspx
const AcceleratorMapping kAcceleratorMap[] = {
  { ui::VKEY_LEFT, ui::EF_ALT_DOWN, IDC_BACK },
  { ui::VKEY_BACK, ui::EF_NONE, IDC_BACK },
  { ui::VKEY_D, ui::EF_CONTROL_DOWN, IDC_BOOKMARK_PAGE },
  { ui::VKEY_D, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_BOOKMARK_ALL_TABS },
  { ui::VKEY_W, ui::EF_CONTROL_DOWN, IDC_CLOSE_TAB },
  { ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_CLOSE_WINDOW },
  { ui::VKEY_F, ui::EF_CONTROL_DOWN, IDC_FIND },
  { ui::VKEY_G, ui::EF_CONTROL_DOWN, IDC_FIND_NEXT },
  { ui::VKEY_G, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_FIND_PREVIOUS },
  { ui::VKEY_D, ui::EF_ALT_DOWN, IDC_FOCUS_LOCATION },
  { ui::VKEY_L, ui::EF_CONTROL_DOWN, IDC_FOCUS_LOCATION },
  { ui::VKEY_K, ui::EF_CONTROL_DOWN, IDC_FOCUS_SEARCH },
  { ui::VKEY_E, ui::EF_CONTROL_DOWN, IDC_FOCUS_SEARCH },
  { ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, IDC_FOCUS_TOOLBAR },
  { ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, IDC_FOCUS_BOOKMARKS },
  { ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, IDC_FOCUS_INFOBARS },
  { ui::VKEY_RIGHT, ui::EF_ALT_DOWN, IDC_FORWARD },
  { ui::VKEY_BACK, ui::EF_SHIFT_DOWN, IDC_FORWARD },
  { ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_DEV_TOOLS },
  { ui::VKEY_F12, ui::EF_NONE, IDC_DEV_TOOLS_TOGGLE },
  { ui::VKEY_J, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_DEV_TOOLS_CONSOLE },
  { ui::VKEY_C, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_DEV_TOOLS_INSPECT },
  { ui::VKEY_O, ui::EF_CONTROL_DOWN, IDC_OPEN_FILE },
  { ui::VKEY_P, ui::EF_CONTROL_DOWN, IDC_PRINT},
#if !defined(DISABLE_BASIC_PRINTING)
  { ui::VKEY_P, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_BASIC_PRINT},
#endif  // !DISABLE_BASIC_PRINTING
  { ui::VKEY_R, ui::EF_CONTROL_DOWN, IDC_RELOAD },
  { ui::VKEY_R, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_RELOAD_IGNORING_CACHE },
  { ui::VKEY_HOME, ui::EF_ALT_DOWN, IDC_HOME },
  { ui::VKEY_S, ui::EF_CONTROL_DOWN, IDC_SAVE_PAGE },
  { ui::VKEY_9, ui::EF_CONTROL_DOWN, IDC_SELECT_LAST_TAB },
  { ui::VKEY_NUMPAD9, ui::EF_CONTROL_DOWN, IDC_SELECT_LAST_TAB },
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  { ui::VKEY_9, ui::EF_ALT_DOWN, IDC_SELECT_LAST_TAB },
  { ui::VKEY_NUMPAD9, ui::EF_ALT_DOWN, IDC_SELECT_LAST_TAB },
  { ui::VKEY_NEXT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, IDC_MOVE_TAB_NEXT },
  { ui::VKEY_PRIOR, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
    IDC_MOVE_TAB_PREVIOUS },
#endif
  { ui::VKEY_TAB, ui::EF_CONTROL_DOWN, IDC_SELECT_NEXT_TAB },
  { ui::VKEY_NEXT, ui::EF_CONTROL_DOWN, IDC_SELECT_NEXT_TAB },
  { ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_SELECT_PREVIOUS_TAB },
  { ui::VKEY_PRIOR, ui::EF_CONTROL_DOWN, IDC_SELECT_PREVIOUS_TAB },
  { ui::VKEY_1, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_0 },
  { ui::VKEY_NUMPAD1, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_0 },
  { ui::VKEY_2, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_1 },
  { ui::VKEY_NUMPAD2, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_1 },
  { ui::VKEY_3, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_2 },
  { ui::VKEY_NUMPAD3, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_2 },
  { ui::VKEY_4, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_3 },
  { ui::VKEY_NUMPAD4, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_3 },
  { ui::VKEY_5, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_4 },
  { ui::VKEY_NUMPAD5, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_4 },
  { ui::VKEY_6, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_5 },
  { ui::VKEY_NUMPAD6, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_5 },
  { ui::VKEY_7, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_6 },
  { ui::VKEY_NUMPAD7, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_6 },
  { ui::VKEY_8, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_7 },
  { ui::VKEY_NUMPAD8, ui::EF_CONTROL_DOWN, IDC_SELECT_TAB_7 },
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  { ui::VKEY_1, ui::EF_ALT_DOWN, IDC_SELECT_TAB_0 },
  { ui::VKEY_NUMPAD1, ui::EF_ALT_DOWN, IDC_SELECT_TAB_0 },
  { ui::VKEY_2, ui::EF_ALT_DOWN, IDC_SELECT_TAB_1 },
  { ui::VKEY_NUMPAD2, ui::EF_ALT_DOWN, IDC_SELECT_TAB_1 },
  { ui::VKEY_3, ui::EF_ALT_DOWN, IDC_SELECT_TAB_2 },
  { ui::VKEY_NUMPAD3, ui::EF_ALT_DOWN, IDC_SELECT_TAB_2 },
  { ui::VKEY_4, ui::EF_ALT_DOWN, IDC_SELECT_TAB_3 },
  { ui::VKEY_NUMPAD4, ui::EF_ALT_DOWN, IDC_SELECT_TAB_3 },
  { ui::VKEY_5, ui::EF_ALT_DOWN, IDC_SELECT_TAB_4 },
  { ui::VKEY_NUMPAD5, ui::EF_ALT_DOWN, IDC_SELECT_TAB_4 },
  { ui::VKEY_6, ui::EF_ALT_DOWN, IDC_SELECT_TAB_5 },
  { ui::VKEY_NUMPAD6, ui::EF_ALT_DOWN, IDC_SELECT_TAB_5 },
  { ui::VKEY_7, ui::EF_ALT_DOWN, IDC_SELECT_TAB_6 },
  { ui::VKEY_NUMPAD7, ui::EF_ALT_DOWN, IDC_SELECT_TAB_6 },
  { ui::VKEY_8, ui::EF_ALT_DOWN, IDC_SELECT_TAB_7 },
  { ui::VKEY_NUMPAD8, ui::EF_ALT_DOWN, IDC_SELECT_TAB_7 },
  { ui::VKEY_BROWSER_FAVORITES, ui::EF_NONE, IDC_SHOW_BOOKMARK_BAR },
#endif
  { ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_SHOW_BOOKMARK_BAR },
  { ui::VKEY_O, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_SHOW_BOOKMARK_MANAGER },
  { ui::VKEY_J, ui::EF_CONTROL_DOWN, IDC_SHOW_DOWNLOADS },
  { ui::VKEY_H, ui::EF_CONTROL_DOWN, IDC_SHOW_HISTORY },
  { ui::VKEY_F, ui::EF_ALT_DOWN, IDC_SHOW_APP_MENU},
  { ui::VKEY_E, ui::EF_ALT_DOWN, IDC_SHOW_APP_MENU},
  { ui::VKEY_ESCAPE, ui::EF_NONE, IDC_STOP },
  { ui::VKEY_OEM_PERIOD, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_TOGGLE_SPEECH_INPUT },
  { ui::VKEY_U, ui::EF_CONTROL_DOWN, IDC_VIEW_SOURCE },
  { ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN, IDC_ZOOM_MINUS },
  { ui::VKEY_SUBTRACT, ui::EF_CONTROL_DOWN, IDC_ZOOM_MINUS },
  { ui::VKEY_0, ui::EF_CONTROL_DOWN, IDC_ZOOM_NORMAL },
  { ui::VKEY_NUMPAD0, ui::EF_CONTROL_DOWN, IDC_ZOOM_NORMAL },
  { ui::VKEY_OEM_PLUS, ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_ADD, ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_F1, ui::EF_NONE, IDC_HELP_PAGE_VIA_KEYBOARD },
  { ui::VKEY_F3, ui::EF_NONE, IDC_FIND_NEXT },
  { ui::VKEY_F3, ui::EF_SHIFT_DOWN, IDC_FIND_PREVIOUS },
  { ui::VKEY_F4, ui::EF_CONTROL_DOWN, IDC_CLOSE_TAB },
  { ui::VKEY_F4, ui::EF_ALT_DOWN, IDC_CLOSE_WINDOW },
  { ui::VKEY_F5, ui::EF_NONE, IDC_RELOAD },
  { ui::VKEY_F5, ui::EF_CONTROL_DOWN, IDC_RELOAD_IGNORING_CACHE },
  { ui::VKEY_F5, ui::EF_SHIFT_DOWN, IDC_RELOAD_IGNORING_CACHE },
  { ui::VKEY_F6, ui::EF_NONE, IDC_FOCUS_NEXT_PANE },
  { ui::VKEY_F6, ui::EF_SHIFT_DOWN, IDC_FOCUS_PREVIOUS_PANE },
  { ui::VKEY_F10, ui::EF_NONE, IDC_FOCUS_MENU_BAR },
  { ui::VKEY_F11, ui::EF_NONE, IDC_FULLSCREEN },
  { ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN, IDC_TASK_MANAGER },

  // Platform-specific key maps.
#if defined(OS_LINUX)
  { ui::VKEY_BROWSER_BACK, ui::EF_NONE, IDC_BACK },
  { ui::VKEY_BROWSER_FORWARD, ui::EF_NONE, IDC_FORWARD },
  { ui::VKEY_BROWSER_HOME, ui::EF_NONE, IDC_HOME },
  { ui::VKEY_BROWSER_REFRESH, ui::EF_NONE, IDC_RELOAD },
  { ui::VKEY_BROWSER_REFRESH, ui::EF_CONTROL_DOWN, IDC_RELOAD_IGNORING_CACHE },
  { ui::VKEY_BROWSER_REFRESH, ui::EF_SHIFT_DOWN, IDC_RELOAD_IGNORING_CACHE },
#endif  // defined(OS_LINUX)

#if defined(OS_CHROMEOS)
  // On Chrome OS, VKEY_BROWSER_SEARCH is handled in Ash.
  { ui::VKEY_BACK, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_CLEAR_BROWSING_DATA },
  { ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN, IDC_HELP_PAGE_VIA_KEYBOARD },
  { ui::VKEY_OEM_2, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_HELP_PAGE_VIA_KEYBOARD },
  { ui::VKEY_BROWSER_FAVORITES, ui::EF_NONE, IDC_SHOW_BOOKMARK_MANAGER },
  { ui::VKEY_BROWSER_STOP, ui::EF_NONE, IDC_STOP },
#else  // OS_CHROMEOS
  { ui::VKEY_DELETE, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_CLEAR_BROWSING_DATA },
  { ui::VKEY_LMENU, ui::EF_NONE, IDC_FOCUS_MENU_BAR },
  { ui::VKEY_MENU, ui::EF_NONE, IDC_FOCUS_MENU_BAR },
  { ui::VKEY_RMENU, ui::EF_NONE, IDC_FOCUS_MENU_BAR },
  // On Windows, all VKEY_BROWSER_* keys except VKEY_BROWSER_SEARCH are handled
  // via WM_APPCOMMAND.
  { ui::VKEY_BROWSER_SEARCH, ui::EF_NONE, IDC_FOCUS_SEARCH },
  { ui::VKEY_M, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_SHOW_AVATAR_MENU},
  // On ChromeOS, these keys are assigned to change UI scale.
  { ui::VKEY_OEM_PLUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_OEM_MINUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_ZOOM_MINUS },
  // For each entry here add an entry into kChromeCmdId2AshActionId below
  // if Ash has a corresponding accelerator.
#if defined(GOOGLE_CHROME_BUILD)
  { ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, IDC_FEEDBACK },
#endif
  { ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_EXIT },
  { ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_NEW_INCOGNITO_WINDOW },
  { ui::VKEY_T, ui::EF_CONTROL_DOWN, IDC_NEW_TAB },
  { ui::VKEY_N, ui::EF_CONTROL_DOWN, IDC_NEW_WINDOW },
  { ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_RESTORE_TAB },
#endif
};
const size_t kAcceleratorMapLength = arraysize(kAcceleratorMap);

#if defined(USE_ASH)
// Below we map Chrome command ids to Ash action ids for commands that have
// an shortcut that is handled by Ash (instead of Chrome). Adding entries
// here will show shortcut text on menus. See comment above.
struct ChromeCmdId2AshActionId {
  const int chrome_cmd_id;
  const ash::AcceleratorAction ash_action_id;
};
const ChromeCmdId2AshActionId kChromeCmdId2AshActionId[] = {
#if defined(GOOGLE_CHROME_BUILD)
  { IDC_FEEDBACK,             ash::OPEN_FEEDBACK_PAGE },
#endif
  { IDC_EXIT,                 ash::EXIT },
  { IDC_NEW_INCOGNITO_WINDOW, ash::NEW_INCOGNITO_WINDOW },
  { IDC_NEW_TAB,              ash::NEW_TAB },
  { IDC_NEW_WINDOW,           ash::NEW_WINDOW },
  { IDC_RESTORE_TAB,          ash::RESTORE_TAB },
  { IDC_TASK_MANAGER,         ash::SHOW_TASK_MANAGER },
};
const size_t kChromeCmdId2AshActionIdLength =
    arraysize(kChromeCmdId2AshActionId);
#endif // defined(USE_ASH)

} // namespace

std::vector<AcceleratorMapping> GetAcceleratorList() {
  return std::vector<AcceleratorMapping>(
      kAcceleratorMap, kAcceleratorMap + kAcceleratorMapLength);
}

bool GetAshAcceleratorForCommandId(int command_id,
                                   HostDesktopType host_desktop_type,
                                   ui::Accelerator* accelerator) {
#if defined(USE_ASH)
  if (host_desktop_type != chrome::HOST_DESKTOP_TYPE_ASH)
    return false;
  for (size_t i = 0; i <  kChromeCmdId2AshActionIdLength; ++i) {
    if (command_id == kChromeCmdId2AshActionId[i].chrome_cmd_id) {
      for (size_t j = 0; j < ash::kAcceleratorDataLength; ++j) {
        if (kChromeCmdId2AshActionId[i].ash_action_id ==
            ash::kAcceleratorData[j].action) {
          *accelerator = ui::Accelerator(ash::kAcceleratorData[j].keycode,
                                         ash::kAcceleratorData[j].modifiers);
          return true;
        }
      }
    }
  }
#endif // defined(USE_ASH)
  return false;
}

bool GetStandardAcceleratorForCommandId(int command_id,
                                        ui::Accelerator* accelerator) {
  // The standard Ctrl-X, Ctrl-V and Ctrl-C are not defined as accelerators
  // anywhere else.
  switch (command_id) {
    case IDC_CUT:
      *accelerator = ui::Accelerator(ui::VKEY_X, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_COPY:
      *accelerator = ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN);
      return true;
    case IDC_PASTE:
      *accelerator = ui::Accelerator(ui::VKEY_V, ui::EF_CONTROL_DOWN);
      return true;
  }
  return false;
}

}  // namespace chrome
