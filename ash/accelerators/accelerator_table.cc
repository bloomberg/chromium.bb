// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_table.h"

#include "base/basictypes.h"

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
  // Single shift release turns off caps lock.
  { false, ui::VKEY_LSHIFT, ui::EF_NONE, DISABLE_CAPS_LOCK },
  { false, ui::VKEY_SHIFT, ui::EF_NONE, DISABLE_CAPS_LOCK },
  { false, ui::VKEY_RSHIFT, ui::EF_NONE, DISABLE_CAPS_LOCK },

  { true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN, PREVIOUS_IME },
  // Shortcuts for Japanese IME.
  { true, ui::VKEY_CONVERT, ui::EF_NONE, SWITCH_IME },
  { true, ui::VKEY_NONCONVERT, ui::EF_NONE, SWITCH_IME },
  { true, ui::VKEY_DBE_SBCSCHAR, ui::EF_NONE, SWITCH_IME },
  { true, ui::VKEY_DBE_DBCSCHAR, ui::EF_NONE, SWITCH_IME },
  // Shortcut for Koren IME.
  { true, ui::VKEY_HANGUL, ui::EF_NONE, SWITCH_IME },

  { true, ui::VKEY_TAB, ui::EF_ALT_DOWN, CYCLE_FORWARD_MRU },
  { true, ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
    CYCLE_BACKWARD_MRU },
  { true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE,
      CYCLE_FORWARD_LINEAR },
#if defined(OS_CHROMEOS)
  { true, ui::VKEY_BROWSER_SEARCH, ui::EF_NONE, TOGGLE_APP_LIST },
  { true, ui::VKEY_WLAN, ui::EF_NONE, TOGGLE_WIFI },
  { true, ui::VKEY_KBD_BRIGHTNESS_DOWN, ui::EF_NONE, KEYBOARD_BRIGHTNESS_DOWN },
  { true, ui::VKEY_KBD_BRIGHTNESS_UP, ui::EF_NONE, KEYBOARD_BRIGHTNESS_UP },
  // Maximize button.
  { true, ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_CONTROL_DOWN, CYCLE_DISPLAY_MODE },
  { true, ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_ALT_DOWN, SWAP_PRIMARY_DISPLAY },
  // Cycle windows button.
  { true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN, TAKE_SCREENSHOT },
  { true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    TAKE_PARTIAL_SCREENSHOT },
  { true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE, BRIGHTNESS_DOWN },
  { true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_ALT_DOWN, KEYBOARD_BRIGHTNESS_DOWN },
  { true, ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE, BRIGHTNESS_UP },
  { true, ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN, KEYBOARD_BRIGHTNESS_UP },
  { true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    MAGNIFY_SCREEN_ZOOM_OUT},
  { true, ui::VKEY_BRIGHTNESS_UP, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    MAGNIFY_SCREEN_ZOOM_IN},
  { true, ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, LOCK_SCREEN },
  // The lock key on Chrome OS keyboards produces F13 scancodes.
  { true, ui::VKEY_F13, ui::EF_NONE, LOCK_PRESSED },
  { false, ui::VKEY_F13, ui::EF_NONE, LOCK_RELEASED },
  { true, ui::VKEY_POWER, ui::EF_NONE, POWER_PRESSED },
  { false, ui::VKEY_POWER, ui::EF_NONE, POWER_RELEASED },
#if !defined(NDEBUG)
  // Extra shortcut for debug build to control magnifier on linux desktop.
  { true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_CONTROL_DOWN,
    MAGNIFY_SCREEN_ZOOM_OUT},
  { true, ui::VKEY_BRIGHTNESS_UP, ui::EF_CONTROL_DOWN, MAGNIFY_SCREEN_ZOOM_IN},
  // Extra shortcut for debug build to activate lock screen on linux desktop.
  { true, ui::VKEY_L, ui::EF_ALT_DOWN, LOCK_SCREEN },
  // Extra shortcut for display swaping as alt-f4 is taken on linux desktop.
  { true, ui::VKEY_F4, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
    SWAP_PRIMARY_DISPLAY },
  // Extra shortcut to lock the screen on linux desktop.
  { true, ui::VKEY_POWER, ui::EF_SHIFT_DOWN, LOCK_PRESSED },
  { false, ui::VKEY_POWER, ui::EF_SHIFT_DOWN, LOCK_RELEASED },
#endif  // !defined(NDEBUG)
  { true, ui::VKEY_O, ui::EF_CONTROL_DOWN, OPEN_FILE_DIALOG },
  { true, ui::VKEY_M, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    OPEN_FILE_MANAGER },
  { true, ui::VKEY_T, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, OPEN_CROSH },
  { true, ui::VKEY_G, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    DISABLE_GPU_WATCHDOG },
#endif  // defined(OS_CHROMEOS)
  { true, ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, OPEN_FEEDBACK_PAGE },
#if !defined(OS_WIN)
  { true, ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, EXIT },
#endif
  { true, ui::VKEY_Z, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    TOGGLE_SPOKEN_FEEDBACK },
  { true, ui::VKEY_I, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    TOUCH_HUD_MODE_CHANGE },
  { true, ui::VKEY_I, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
    TOUCH_HUD_CLEAR },

  // When you change the shortcuts for NEW_INCOGNITO_WINDOW, NEW_WINDOW, or
  // NEW_TAB, you also need to modify
  // ToolbarView::GetAcceleratorForCommandId() in
  // chrome/browser/ui/views/toolbar_view.cc.
  { true, ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    NEW_INCOGNITO_WINDOW },
  { true, ui::VKEY_N, ui::EF_CONTROL_DOWN, NEW_WINDOW },
  { true, ui::VKEY_T, ui::EF_CONTROL_DOWN, NEW_TAB },

  { true, ui::VKEY_BROWSER_REFRESH,
    ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    ROTATE_WINDOW },
  { true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_SHIFT_DOWN,
    CYCLE_BACKWARD_LINEAR },
  { true, ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, RESTORE_TAB },
  { true, ui::VKEY_PRINT, ui::EF_NONE, TAKE_SCREENSHOT },
  // On Chrome OS, Search key is mapped to LWIN. The Search key binding should
  // act on release instead of press when using Search as a modifier key for
  // extended keyboard shortcuts.
  { false, ui::VKEY_LWIN, ui::EF_NONE, TOGGLE_APP_LIST },
  { false, ui::VKEY_LWIN, ui::EF_ALT_DOWN, TOGGLE_CAPS_LOCK },
  { true, ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_NONE, TOGGLE_MAXIMIZED },
  { true, ui::VKEY_VOLUME_MUTE, ui::EF_NONE, VOLUME_MUTE },
  { true, ui::VKEY_VOLUME_DOWN, ui::EF_NONE, VOLUME_DOWN },
  { true, ui::VKEY_VOLUME_UP, ui::EF_NONE, VOLUME_UP },
  { true, ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, FOCUS_LAUNCHER },
  { true, ui::VKEY_HELP, ui::EF_NONE, SHOW_KEYBOARD_OVERLAY },
  { true, ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    SHOW_KEYBOARD_OVERLAY },
  { true, ui::VKEY_OEM_2,
    ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    SHOW_KEYBOARD_OVERLAY },
  { true, ui::VKEY_F14, ui::EF_NONE, SHOW_KEYBOARD_OVERLAY },
  { true, ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
    SHOW_MESSAGE_CENTER_BUBBLE },
  { true, ui::VKEY_BROWSER_BACK, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    SHOW_OAK },
  { true, ui::VKEY_S, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
    SHOW_SYSTEM_TRAY_BUBBLE },
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
  // Convenience for users switching from Mac OS.
  { true, ui::VKEY_M, ui::EF_CONTROL_DOWN, WINDOW_MINIMIZE },
  { true, ui::VKEY_OEM_PLUS, ui::EF_ALT_DOWN, TOGGLE_MAXIMIZED },
  { true, ui::VKEY_OEM_PLUS, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
    WINDOW_POSITION_CENTER },
  { true, ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN, FOCUS_NEXT_PANE },
  { true, ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN, FOCUS_PREVIOUS_PANE },

  // Media Player shortcuts.
  { true, ui::VKEY_MEDIA_NEXT_TRACK, ui::EF_NONE, MEDIA_NEXT_TRACK},
  { true, ui::VKEY_MEDIA_PLAY_PAUSE, ui::EF_NONE, MEDIA_PLAY_PAUSE},
  { true, ui::VKEY_MEDIA_PREV_TRACK, ui::EF_NONE, MEDIA_PREV_TRACK},

  // TODO(yusukes): Handle VKEY_MEDIA_STOP, and
  // VKEY_MEDIA_LAUNCH_MAIL.
};

const size_t kAcceleratorDataLength = arraysize(kAcceleratorData);

const AcceleratorData kDebugAcceleratorData[] = {
  { true, ui::VKEY_HOME, ui::EF_CONTROL_DOWN, ROTATE_SCREEN },
  { true, ui::VKEY_B, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    TOGGLE_DESKTOP_BACKGROUND_MODE },
  { true, ui::VKEY_F11, ui::EF_CONTROL_DOWN, TOGGLE_ROOT_WINDOW_FULL_SCREEN },
  { true, ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, TOGGLE_WIFI },
  // For testing on systems where Alt-Tab is already mapped.
  { true, ui::VKEY_W, ui::EF_ALT_DOWN, CYCLE_FORWARD_MRU },
  { true, ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
      CYCLE_BACKWARD_MRU },
  { true, ui::VKEY_HOME, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    DISPLAY_TOGGLE_SCALE },
#if !defined(NDEBUG)
  { true, ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    PRINT_LAYER_HIERARCHY },
  { true, ui::VKEY_V, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    PRINT_VIEW_HIERARCHY },
  { true, ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    PRINT_WINDOW_HIERARCHY },
#endif
};

const size_t kDebugAcceleratorDataLength = arraysize(kDebugAcceleratorData);

const AcceleratorAction kReservedActions[] = {
  // Window cycling accelerators.
  CYCLE_BACKWARD_MRU,  // Shift+Alt+Tab
  CYCLE_FORWARD_MRU,  // Alt+Tab
#if defined(OS_CHROMEOS)
  POWER_PRESSED,
  POWER_RELEASED,
#endif
};

const size_t kReservedActionsLength = arraysize(kReservedActions);

const AcceleratorAction kActionsAllowedAtLoginOrLockScreen[] = {
  BRIGHTNESS_DOWN,
  BRIGHTNESS_UP,
#if defined(OS_CHROMEOS)
  CYCLE_DISPLAY_MODE,
  DISABLE_GPU_WATCHDOG,
#endif  // defined(OS_CHROMEOS)
  DISABLE_CAPS_LOCK,
  KEYBOARD_BRIGHTNESS_DOWN,
  KEYBOARD_BRIGHTNESS_UP,
  MAGNIFY_SCREEN_ZOOM_IN,  // Control+F7
  MAGNIFY_SCREEN_ZOOM_OUT,  // Control+F6
  NEXT_IME,
  PREVIOUS_IME,
  SWITCH_IME,  // Switch to another IME depending on the accelerator.
  TAKE_SCREENSHOT,
  TAKE_PARTIAL_SCREENSHOT,
  TOGGLE_CAPS_LOCK,
  TOGGLE_SPOKEN_FEEDBACK,
  TOGGLE_WIFI,
  TOUCH_HUD_CLEAR,
  VOLUME_DOWN,
  VOLUME_MUTE,
  VOLUME_UP,
  ROTATE_WINDOW,
#if !defined(NDEBUG)
  PRINT_LAYER_HIERARCHY,
  PRINT_VIEW_HIERARCHY,
  PRINT_WINDOW_HIERARCHY,
  ROTATE_SCREEN,
#endif
#if defined(OS_CHROMEOS) && !defined(NDEBUG)
  POWER_PRESSED,
  POWER_RELEASED,
#endif
};

const size_t kActionsAllowedAtLoginOrLockScreenLength =
    arraysize(kActionsAllowedAtLoginOrLockScreen);

const AcceleratorAction kActionsAllowedAtLockScreen[] = {
  EXIT,
};

const size_t kActionsAllowedAtLockScreenLength =
    arraysize(kActionsAllowedAtLockScreen);

const AcceleratorAction kActionsAllowedAtModalWindow[] = {
  BRIGHTNESS_DOWN,
  BRIGHTNESS_UP,
  DISABLE_CAPS_LOCK,
  EXIT,
  KEYBOARD_BRIGHTNESS_DOWN,
  KEYBOARD_BRIGHTNESS_UP,
  MAGNIFY_SCREEN_ZOOM_IN,
  MAGNIFY_SCREEN_ZOOM_OUT,
  MEDIA_NEXT_TRACK,
  MEDIA_PLAY_PAUSE,
  MEDIA_PREV_TRACK,
  NEXT_IME,
  OPEN_FEEDBACK_PAGE,
  POWER_PRESSED,
  POWER_RELEASED,
  PREVIOUS_IME,
  SHOW_KEYBOARD_OVERLAY,
  SWAP_PRIMARY_DISPLAY,
  SWITCH_IME,
  TAKE_SCREENSHOT,
  TAKE_PARTIAL_SCREENSHOT,
  TOGGLE_CAPS_LOCK,
  TOGGLE_SPOKEN_FEEDBACK,
  TOGGLE_WIFI,
  VOLUME_DOWN,
  VOLUME_MUTE,
  VOLUME_UP,
#if defined(OS_CHROMEOS)
  CYCLE_DISPLAY_MODE,
  LOCK_SCREEN,
#endif
};

const size_t kActionsAllowedAtModalWindowLength =
    arraysize(kActionsAllowedAtModalWindow);

const AcceleratorAction kNonrepeatableActions[] = {
  // TODO(mazda): Add other actions which should not be repeated.
  CYCLE_BACKWARD_LINEAR,
  CYCLE_BACKWARD_MRU,
  CYCLE_FORWARD_LINEAR,
  CYCLE_FORWARD_MRU,
  TOGGLE_MAXIMIZED,
  WINDOW_MINIMIZE,
};

const size_t kNonrepeatableActionsLength =
    arraysize(kNonrepeatableActions);

}  // namespace ash
