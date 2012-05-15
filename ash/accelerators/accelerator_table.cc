// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_table.h"

#include "base/basictypes.h"

namespace ash {

const AcceleratorData kAcceleratorData[] = {
  // trigger_on_press, KeyboardCode, shift, control, alt, AcceleratorAction

  { false, ui::VKEY_MENU, true, false, true, NEXT_IME },
  { false, ui::VKEY_SHIFT, true, false, true, NEXT_IME },
  { true, ui::VKEY_SPACE, false, true, false, PREVIOUS_IME },
  // Shortcuts for Japanese IME.
  { true, ui::VKEY_CONVERT, false, false, false, SWITCH_IME },
  { true, ui::VKEY_NONCONVERT, false, false, false, SWITCH_IME },
  { true, ui::VKEY_DBE_SBCSCHAR, false, false, false, SWITCH_IME },
  { true, ui::VKEY_DBE_DBCSCHAR, false, false, false, SWITCH_IME },
  // Shortcuts for Koren IME.
  { true, ui::VKEY_HANGUL, false, false, false, SWITCH_IME },
  { true, ui::VKEY_SPACE, true, false, false, SWITCH_IME },

  { true, ui::VKEY_TAB, false, false, true, CYCLE_FORWARD_MRU },
  { true, ui::VKEY_TAB, true, false, true, CYCLE_BACKWARD_MRU },
  { true, ui::VKEY_F5, false, false, false, CYCLE_FORWARD_LINEAR },
#if defined(OS_CHROMEOS)
  { true, ui::VKEY_BRIGHTNESS_DOWN, false, false, false, BRIGHTNESS_DOWN },
  { true, ui::VKEY_BRIGHTNESS_UP, false, false, false, BRIGHTNESS_UP },
  { true, ui::VKEY_L, true, true, false, LOCK_SCREEN },
  { true, ui::VKEY_M, false, true, false, OPEN_FILE_MANAGER },
  { true, ui::VKEY_T, false, true, true, OPEN_CROSH },
#endif
  { true, ui::VKEY_Q, true, true, false, EXIT },
  { true, ui::VKEY_Z, false, true, true, TOGGLE_SPOKEN_FEEDBACK },

  // When you change the shortcut for NEW_INCOGNITO_WINDOW or NEW_WINDOW,
  // you also need to modify ToolbarView::GetAcceleratorForCommandId() in
  // chrome/browser/ui/views/toolbar_view.cc.
  { true, ui::VKEY_N, true, true, false, NEW_INCOGNITO_WINDOW },
  { true, ui::VKEY_N, false, true, false, NEW_WINDOW },

  { true, ui::VKEY_F5, true, false, false, CYCLE_BACKWARD_LINEAR },
  { true, ui::VKEY_F5, false, true, false, TAKE_SCREENSHOT },
  { true, ui::VKEY_F5, true, true, false, TAKE_PARTIAL_SCREENSHOT },
  { true, ui::VKEY_PRINT, false, false, false, TAKE_SCREENSHOT },
  // On Chrome OS, Search key is mapped to LWIN.
  { true, ui::VKEY_LWIN, false, false, false, SEARCH_KEY },
  { true, ui::VKEY_LWIN, false, true, false, TOGGLE_APP_LIST },
  { true, ui::VKEY_LWIN, true, false, false, TOGGLE_CAPS_LOCK },
  { true, ui::VKEY_F6, false, false, false, BRIGHTNESS_DOWN },
  { true, ui::VKEY_F7, false, false, false, BRIGHTNESS_UP },
  { true, ui::VKEY_F8, false, false, false, VOLUME_MUTE },
  { true, ui::VKEY_VOLUME_MUTE, false, false, false, VOLUME_MUTE },
  { true, ui::VKEY_F9, false, false, false, VOLUME_DOWN },
  { true, ui::VKEY_VOLUME_DOWN, false, false, false, VOLUME_DOWN },
  { true, ui::VKEY_F10, false, false, false, VOLUME_UP },
  { true, ui::VKEY_VOLUME_UP, false, false, false, VOLUME_UP },
  { true, ui::VKEY_L, true, false, true, FOCUS_LAUNCHER },
  { true, ui::VKEY_S, true, false, true, FOCUS_TRAY },
  { true, ui::VKEY_OEM_2, false, true, true, SHOW_KEYBOARD_OVERLAY },
  { true, ui::VKEY_OEM_2, true, true, true, SHOW_KEYBOARD_OVERLAY },
  { true, ui::VKEY_F1, true, true, false, SHOW_OAK },
  { true, ui::VKEY_1, false, false, true, SELECT_WIN_0 },
  { true, ui::VKEY_2, false, false, true, SELECT_WIN_1 },
  { true, ui::VKEY_3, false, false, true, SELECT_WIN_2 },
  { true, ui::VKEY_4, false, false, true, SELECT_WIN_3 },
  { true, ui::VKEY_5, false, false, true, SELECT_WIN_4 },
  { true, ui::VKEY_6, false, false, true, SELECT_WIN_5 },
  { true, ui::VKEY_7, false, false, true, SELECT_WIN_6 },
  { true, ui::VKEY_8, false, false, true, SELECT_WIN_7 },
  { true, ui::VKEY_9, false, false, true, SELECT_LAST_WIN },

  // We need the number keys with and without shift since the French keyboard
  // does not have explicit number keys. Instead they have to press
  // 'Shift' + .. to access the keys. If we ever have an overlap of
  // functionality, we should think about either assembling this table
  // dynamically - or by decoding the keys properly (which is of course in
  // conflict with other keyboards since the Shift+ is missing then).
  { true, ui::VKEY_1, true, false, true, SELECT_WIN_0 },
  { true, ui::VKEY_2, true, false, true, SELECT_WIN_1 },
  { true, ui::VKEY_3, true, false, true, SELECT_WIN_2 },
  { true, ui::VKEY_4, true, false, true, SELECT_WIN_3 },
  { true, ui::VKEY_5, true, false, true, SELECT_WIN_4 },
  { true, ui::VKEY_6, true, false, true, SELECT_WIN_5 },
  { true, ui::VKEY_7, true, false, true, SELECT_WIN_6 },
  { true, ui::VKEY_8, true, false, true, SELECT_WIN_7 },
  { true, ui::VKEY_9, true, false, true, SELECT_LAST_WIN },

  // Window management shortcuts.
  { true, ui::VKEY_OEM_4, false, false, true, WINDOW_SNAP_LEFT },
  { true, ui::VKEY_OEM_6, false, false, true, WINDOW_SNAP_RIGHT },
  { true, ui::VKEY_OEM_MINUS, false, false, true, WINDOW_MINIMIZE },
  { true, ui::VKEY_OEM_PLUS, false, false, true, WINDOW_MAXIMIZE_RESTORE },
  { true, ui::VKEY_OEM_PLUS, true, false, true, WINDOW_POSITION_CENTER },

  { true, ui::VKEY_F3, true, true, true, ROTATE_WINDOWS },
#if !defined(NDEBUG)
  { true, ui::VKEY_HOME, false, true, false, ROTATE_SCREEN },
  { true, ui::VKEY_B, false, true, true, TOGGLE_DESKTOP_BACKGROUND_MODE },
  { true, ui::VKEY_F11, false, true, false, TOGGLE_ROOT_WINDOW_FULL_SCREEN },
  { true, ui::VKEY_L, true, true, true, PRINT_LAYER_HIERARCHY },
  { true, ui::VKEY_W, true, true, true, PRINT_WINDOW_HIERARCHY },
  // For testing on systems where Alt-Tab is already mapped.
  { true, ui::VKEY_W, false, false, true, CYCLE_FORWARD_MRU },
  { true, ui::VKEY_W, true, false, true, CYCLE_BACKWARD_MRU },
  { true, ui::VKEY_F4, false, true, false, MONITOR_ADD_REMOVE },
  { true, ui::VKEY_F4, true, true, false, MONITOR_CYCLE },
  { true, ui::VKEY_HOME, true, true, false, MONITOR_TOGGLE_SCALE },
#endif
  // trigger_on_press, KeyboardCode, shift, control, alt, AcceleratorAction
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
