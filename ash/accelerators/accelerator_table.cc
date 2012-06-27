// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_table.h"

#include "base/basictypes.h"
#include "ui/base/events.h"

namespace ash {

const AcceleratorData kAcceleratorData[] = {
  // We have to define 3 entries for Shift+Alt. VKEY_[LR]MENU might be sent to
  // the accelerator controller when RenderWidgetHostViewAura is focused, and
  // VKEY_MENU might be when it's not (e.g. when NativeWidgetAura is focused).
  { false, ui::VKEY_LMENU, ui::EF_SHIFT_DOWN, NEXT_IME },
  { false, ui::VKEY_MENU, ui::EF_SHIFT_DOWN, NEXT_IME },
  { false, ui::VKEY_RMENU, ui::EF_SHIFT_DOWN, NEXT_IME },
  // The same is true for Alt+Shift.
  { false, ui::VKEY_LSHIFT, ui::EF_ALT_DOWN, NEXT_IME },
  { false, ui::VKEY_SHIFT, ui::EF_ALT_DOWN, NEXT_IME },
  { false, ui::VKEY_RSHIFT, ui::EF_ALT_DOWN, NEXT_IME },
#if defined(OS_CHROMEOS)
  // When X11 is in use, a modifier-only accelerator like Shift+Alt could be
  // sent to the accelerator controller in unnormalized form (e.g. when
  // NativeWidgetAura is focused). To handle such accelerators, the following 2
  // entries are necessary. For more details, see crbug.com/127142#c8 and #c14.
  { false, ui::VKEY_MENU, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, NEXT_IME },
  { false, ui::VKEY_SHIFT, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, NEXT_IME },
#endif
  { true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN, PREVIOUS_IME },
  // Shortcuts for Japanese IME.
  { true, ui::VKEY_CONVERT, ui::EF_NONE, SWITCH_IME },
  { true, ui::VKEY_NONCONVERT, ui::EF_NONE, SWITCH_IME },
  { true, ui::VKEY_DBE_SBCSCHAR, ui::EF_NONE, SWITCH_IME },
  { true, ui::VKEY_DBE_DBCSCHAR, ui::EF_NONE, SWITCH_IME },
  // Shortcut for Koren IME.
  { true, ui::VKEY_HANGUL, ui::EF_NONE, SWITCH_IME },

  { true, ui::VKEY_TAB,
    ui::EF_ALT_DOWN, CYCLE_FORWARD_MRU },
  { true, ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
    CYCLE_BACKWARD_MRU },
  { true, ui::VKEY_F5, ui::EF_NONE, CYCLE_FORWARD_LINEAR },
#if defined(OS_CHROMEOS)
  { true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE, BRIGHTNESS_DOWN },
  { true, ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE, BRIGHTNESS_UP },
  { true, ui::VKEY_F4, ui::EF_CONTROL_DOWN, CYCLE_DISPLAY_MODE },
  { true, ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, LOCK_SCREEN },
  { true, ui::VKEY_O, ui::EF_CONTROL_DOWN, OPEN_FILE_MANAGER_DIALOG },
  { true, ui::VKEY_M, ui::EF_CONTROL_DOWN, OPEN_FILE_MANAGER_TAB },
  { true, ui::VKEY_T, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, OPEN_CROSH },
#endif
  { true, ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, EXIT },
  { true, ui::VKEY_Z, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    TOGGLE_SPOKEN_FEEDBACK },

  // When you change the shortcuts for NEW_INCOGNITO_WINDOW, NEW_WINDOW, or
  // NEW_TAB, you also need to modify
  // ToolbarView::GetAcceleratorForCommandId() in
  // chrome/browser/ui/views/toolbar_view.cc.
  { true, ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    NEW_INCOGNITO_WINDOW },
  { true, ui::VKEY_N, ui::EF_CONTROL_DOWN, NEW_WINDOW },
  { true, ui::VKEY_T, ui::EF_CONTROL_DOWN, NEW_TAB },

  { true, ui::VKEY_F5, ui::EF_SHIFT_DOWN, CYCLE_BACKWARD_LINEAR },
  { true, ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, RESTORE_TAB },
  { true, ui::VKEY_F5, ui::EF_CONTROL_DOWN, TAKE_SCREENSHOT },
  { true, ui::VKEY_F5, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    TAKE_PARTIAL_SCREENSHOT },
  { true, ui::VKEY_PRINT, ui::EF_NONE, TAKE_SCREENSHOT },
  // On Chrome OS, Search key is mapped to LWIN.
  { true, ui::VKEY_LWIN, ui::EF_NONE, TOGGLE_APP_LIST },
  { true, ui::VKEY_BROWSER_SEARCH, ui::EF_NONE, TOGGLE_APP_LIST },
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
  { true, ui::VKEY_S, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, FOCUS_SYSTEM_TRAY },
  { true, ui::VKEY_F7, ui::EF_CONTROL_DOWN, MAGNIFY_SCREEN_ZOOM_IN},
  { true, ui::VKEY_F6, ui::EF_CONTROL_DOWN, MAGNIFY_SCREEN_ZOOM_OUT},
  { true, ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    SHOW_KEYBOARD_OVERLAY },
  { true, ui::VKEY_OEM_2,
    ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    SHOW_KEYBOARD_OVERLAY },
  { true, ui::VKEY_F1, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, SHOW_OAK },
  { true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN, SHOW_TASK_MANAGER },
  { true, ui::VKEY_1, ui::EF_ALT_DOWN, SELECT_WIN_0 },
  { true, ui::VKEY_2, ui::EF_ALT_DOWN, SELECT_WIN_1 },
  { true, ui::VKEY_3, ui::EF_ALT_DOWN, SELECT_WIN_2 },
  { true, ui::VKEY_4, ui::EF_ALT_DOWN, SELECT_WIN_3 },
  { true, ui::VKEY_5, ui::EF_ALT_DOWN, SELECT_WIN_4 },
  { true, ui::VKEY_6, ui::EF_ALT_DOWN, SELECT_WIN_5 },
  { true, ui::VKEY_7, ui::EF_ALT_DOWN, SELECT_WIN_6 },
  { true, ui::VKEY_8, ui::EF_ALT_DOWN, SELECT_WIN_7 },
  { true, ui::VKEY_9, ui::EF_ALT_DOWN, SELECT_LAST_WIN },

  // Window management shortcuts.
  { true, ui::VKEY_OEM_4, ui::EF_ALT_DOWN, WINDOW_SNAP_LEFT },
  { true, ui::VKEY_OEM_6, ui::EF_ALT_DOWN, WINDOW_SNAP_RIGHT },
  { true, ui::VKEY_OEM_MINUS, ui::EF_ALT_DOWN, WINDOW_MINIMIZE },
  { true, ui::VKEY_OEM_PLUS, ui::EF_ALT_DOWN, WINDOW_MAXIMIZE_RESTORE },
  { true, ui::VKEY_OEM_PLUS, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
    WINDOW_POSITION_CENTER },
  { true, ui::VKEY_F2, ui::EF_CONTROL_DOWN, FOCUS_NEXT_PANE },
  { true, ui::VKEY_F1, ui::EF_CONTROL_DOWN, FOCUS_PREVIOUS_PANE },

  { true, ui::VKEY_F3,
    ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    ROTATE_WINDOWS },
  { true, ui::VKEY_HOME, ui::EF_CONTROL_DOWN, ROTATE_SCREEN },
  { true, ui::VKEY_B, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    TOGGLE_DESKTOP_BACKGROUND_MODE },
  { true, ui::VKEY_F11, ui::EF_CONTROL_DOWN, TOGGLE_ROOT_WINDOW_FULL_SCREEN },
  // For testing on systems where Alt-Tab is already mapped.
  { true, ui::VKEY_W, ui::EF_ALT_DOWN, CYCLE_FORWARD_MRU },
  { true, ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU },
  { true, ui::VKEY_F4, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, DISPLAY_CYCLE },
  { true, ui::VKEY_F4, ui::EF_SHIFT_DOWN, DISPLAY_ADD_REMOVE },
  { true, ui::VKEY_HOME, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    DISPLAY_TOGGLE_SCALE },
#if !defined(NDEBUG)
  { true, ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    PRINT_LAYER_HIERARCHY },
  { true, ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    PRINT_WINDOW_HIERARCHY },
#endif

  // TODO(yusukes): Handle VKEY_MEDIA_STOP, VKEY_MEDIA_PLAY_PAUSE,
  // VKEY_MEDIA_LAUNCH_MAIL, and VKEY_MEDIA_LAUNCH_APP2 (aka Calculator button).
};

const size_t kAcceleratorDataLength = arraysize(kAcceleratorData);

const AcceleratorAction kReservedActions[] = {
  CYCLE_BACKWARD_MRU,
  CYCLE_FORWARD_MRU,
};

const size_t kReservedActionsLength = arraysize(kReservedActions);

const AcceleratorAction kActionsAllowedAtLoginOrLockScreen[] = {
  BRIGHTNESS_DOWN,
  BRIGHTNESS_UP,
#if defined(OS_CHROMEOS)
  CYCLE_DISPLAY_MODE,
#endif  // defined(OS_CHROMEOS)
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

const size_t kActionsAllowedAtLoginOrLockScreenLength =
    arraysize(kActionsAllowedAtLoginOrLockScreen);

const AcceleratorAction kActionsAllowedAtLockScreen[] = {
  EXIT,
};

const size_t kActionsAllowedAtLockScreenLength =
    arraysize(kActionsAllowedAtLockScreen);

}  // namespace ash
