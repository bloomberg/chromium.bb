// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_table.h"

#include "base/basictypes.h"
#include "ui/base/events.h"

namespace ash {

const AcceleratorData kAcceleratorData[] = {
  { false, ui::VKEY_MENU, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, NEXT_IME },
  { false, ui::VKEY_SHIFT, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, NEXT_IME },
  { true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN, PREVIOUS_IME },
  // Shortcuts for Japanese IME.
  { true, ui::VKEY_CONVERT, ui::EF_NONE, SWITCH_IME },
  { true, ui::VKEY_NONCONVERT, ui::EF_NONE, SWITCH_IME },
  { true, ui::VKEY_DBE_SBCSCHAR, ui::EF_NONE, SWITCH_IME },
  { true, ui::VKEY_DBE_DBCSCHAR, ui::EF_NONE, SWITCH_IME },
  // Shortcuts for Koren IME.
  { true, ui::VKEY_HANGUL, ui::EF_NONE, SWITCH_IME },
  { true, ui::VKEY_SPACE, ui::EF_SHIFT_DOWN, SWITCH_IME },

  { true, ui::VKEY_TAB,
    ui::EF_ALT_DOWN, CYCLE_FORWARD_MRU },
  { true, ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
    CYCLE_BACKWARD_MRU },
  { true, ui::VKEY_F5, ui::EF_NONE, CYCLE_FORWARD_LINEAR },
#if defined(OS_CHROMEOS)
  { true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE, BRIGHTNESS_DOWN },
  { true, ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE, BRIGHTNESS_UP },
  { true, ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, LOCK_SCREEN },
  { true, ui::VKEY_M, ui::EF_CONTROL_DOWN, OPEN_FILE_MANAGER },
  { true, ui::VKEY_T, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, OPEN_CROSH },
#endif
  { true, ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, EXIT },
  { true, ui::VKEY_Z, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    TOGGLE_SPOKEN_FEEDBACK },

  // When you change the shortcut for NEW_INCOGNITO_WINDOW or NEW_WINDOW,
  // you also need to modify ToolbarView::GetAcceleratorForCommandId() in
  // chrome/browser/ui/views/toolbar_view.cc.
  { true, ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    NEW_INCOGNITO_WINDOW },
  { true, ui::VKEY_N, ui::EF_CONTROL_DOWN, NEW_WINDOW },

  { true, ui::VKEY_F5, ui::EF_SHIFT_DOWN, CYCLE_BACKWARD_LINEAR },
  { true, ui::VKEY_F5, ui::EF_CONTROL_DOWN, TAKE_SCREENSHOT },
  { true, ui::VKEY_F5, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    TAKE_PARTIAL_SCREENSHOT },
  { true, ui::VKEY_PRINT, ui::EF_NONE, TAKE_SCREENSHOT },
  // On Chrome OS, Search key is mapped to LWIN.
  { true, ui::VKEY_LWIN, ui::EF_NONE, SEARCH_KEY },
  { true, ui::VKEY_LWIN, ui::EF_CONTROL_DOWN, TOGGLE_APP_LIST },
  { true, ui::VKEY_LWIN, ui::EF_SHIFT_DOWN, TOGGLE_CAPS_LOCK },
  { true, ui::VKEY_F6, ui::EF_NONE, BRIGHTNESS_DOWN },
  { true, ui::VKEY_F7, ui::EF_NONE, BRIGHTNESS_UP },
  { true, ui::VKEY_F8, ui::EF_NONE, VOLUME_MUTE },
  { true, ui::VKEY_VOLUME_MUTE, ui::EF_NONE, VOLUME_MUTE },
  { true, ui::VKEY_F9, ui::EF_NONE, VOLUME_DOWN },
  { true, ui::VKEY_VOLUME_DOWN, ui::EF_NONE, VOLUME_DOWN },
  { true, ui::VKEY_F10, ui::EF_NONE, VOLUME_UP },
  { true, ui::VKEY_VOLUME_UP, ui::EF_NONE, VOLUME_UP },
  { true, ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, FOCUS_LAUNCHER },
  { true, ui::VKEY_S, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, FOCUS_TRAY },
  { true, ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    SHOW_KEYBOARD_OVERLAY },
  { true, ui::VKEY_OEM_2,
    ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    SHOW_KEYBOARD_OVERLAY },
  { true, ui::VKEY_F1, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, SHOW_OAK },
  { true, ui::VKEY_1, ui::EF_ALT_DOWN, SELECT_WIN_0 },
  { true, ui::VKEY_2, ui::EF_ALT_DOWN, SELECT_WIN_1 },
  { true, ui::VKEY_3, ui::EF_ALT_DOWN, SELECT_WIN_2 },
  { true, ui::VKEY_4, ui::EF_ALT_DOWN, SELECT_WIN_3 },
  { true, ui::VKEY_5, ui::EF_ALT_DOWN, SELECT_WIN_4 },
  { true, ui::VKEY_6, ui::EF_ALT_DOWN, SELECT_WIN_5 },
  { true, ui::VKEY_7, ui::EF_ALT_DOWN, SELECT_WIN_6 },
  { true, ui::VKEY_8, ui::EF_ALT_DOWN, SELECT_WIN_7 },
  { true, ui::VKEY_9, ui::EF_ALT_DOWN, SELECT_LAST_WIN },

  // We need the number keys with and without shift since the French keyboard
  // does not have explicit number keys. Instead they have to press
  // 'Shift' + .. to access the keys. If we ever have an overlap of
  // functionality, we should think about either assembling this table
  // dynamically - or by decoding the keys properly (which is of course in
  // conflict with other keyboards since the Shift+ is missing then).
  { true, ui::VKEY_1, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, SELECT_WIN_0 },
  { true, ui::VKEY_2, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, SELECT_WIN_1 },
  { true, ui::VKEY_3, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, SELECT_WIN_2 },
  { true, ui::VKEY_4, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, SELECT_WIN_3 },
  { true, ui::VKEY_5, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, SELECT_WIN_4 },
  { true, ui::VKEY_6, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, SELECT_WIN_5 },
  { true, ui::VKEY_7, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, SELECT_WIN_6 },
  { true, ui::VKEY_8, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, SELECT_WIN_7 },
  { true, ui::VKEY_9, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, SELECT_LAST_WIN },

  // Window management shortcuts.
  { true, ui::VKEY_OEM_4, ui::EF_ALT_DOWN, WINDOW_SNAP_LEFT },
  { true, ui::VKEY_OEM_6, ui::EF_ALT_DOWN, WINDOW_SNAP_RIGHT },
  { true, ui::VKEY_OEM_MINUS, ui::EF_ALT_DOWN, WINDOW_MINIMIZE },
  { true, ui::VKEY_OEM_PLUS, ui::EF_ALT_DOWN, WINDOW_MAXIMIZE_RESTORE },
  { true, ui::VKEY_OEM_PLUS, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
    WINDOW_POSITION_CENTER },

  { true, ui::VKEY_F3,
    ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    ROTATE_WINDOWS },
#if !defined(NDEBUG)
  { true, ui::VKEY_HOME, ui::EF_CONTROL_DOWN, ROTATE_SCREEN },
  { true, ui::VKEY_B, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    TOGGLE_DESKTOP_BACKGROUND_MODE },
  { true, ui::VKEY_F11, ui::EF_CONTROL_DOWN, TOGGLE_ROOT_WINDOW_FULL_SCREEN },
  { true, ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    PRINT_LAYER_HIERARCHY },
  { true, ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    PRINT_WINDOW_HIERARCHY },
  // For testing on systems where Alt-Tab is already mapped.
  { true, ui::VKEY_W, ui::EF_ALT_DOWN, CYCLE_FORWARD_MRU },
  { true, ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU },
  { true, ui::VKEY_F4, ui::EF_CONTROL_DOWN, MONITOR_ADD_REMOVE },
  { true, ui::VKEY_F4, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, MONITOR_CYCLE },
  { true, ui::VKEY_HOME, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    MONITOR_TOGGLE_SCALE },
#endif
};

const size_t kAcceleratorDataLength = arraysize(kAcceleratorData);

const AcceleratorAction kActionsAllowedAtLoginScreen[] = {
  BRIGHTNESS_DOWN,
  BRIGHTNESS_UP,
  NEXT_IME,
  PREVIOUS_IME,
  SWITCH_IME,  // Switch to another IME depending on the accelerator.
  TAKE_SCREENSHOT,
  TAKE_PARTIAL_SCREENSHOT,
  TOGGLE_CAPS_LOCK,
  TOGGLE_SPOKEN_FEEDBACK,
  VOLUME_DOWN,
  VOLUME_MUTE,
  VOLUME_UP,
  ROTATE_WINDOWS,
#if !defined(NDEBUG)
  PRINT_LAYER_HIERARCHY,
  PRINT_WINDOW_HIERARCHY,
  ROTATE_SCREEN,
#endif
};

const size_t kActionsAllowedAtLoginScreenLength =
    arraysize(kActionsAllowedAtLoginScreen);

}  // namespace ash
