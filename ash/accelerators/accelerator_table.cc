// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_table.h"

#include "ash/strings/grit/ash_strings.h"
#include "base/macros.h"

namespace ash {

const AcceleratorData kAcceleratorData[] = {
  { true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN, PREVIOUS_IME },
  { false, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN, PREVIOUS_IME },
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
  { true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE, TOGGLE_OVERVIEW },
#if defined(OS_CHROMEOS)
  { true, ui::VKEY_BROWSER_SEARCH, ui::EF_NONE, TOGGLE_APP_LIST },
  { true, ui::VKEY_WLAN, ui::EF_NONE, TOGGLE_WIFI },
  { true, ui::VKEY_KBD_BRIGHTNESS_DOWN, ui::EF_NONE, KEYBOARD_BRIGHTNESS_DOWN },
  { true, ui::VKEY_KBD_BRIGHTNESS_UP, ui::EF_NONE, KEYBOARD_BRIGHTNESS_UP },
  // Maximize button.
  { true, ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_CONTROL_DOWN, TOGGLE_MIRROR_MODE },
  { true, ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_ALT_DOWN, SWAP_PRIMARY_DISPLAY },
  // Cycle windows button.
  { true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN, TAKE_SCREENSHOT },
  { true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    TAKE_PARTIAL_SCREENSHOT },
  { true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
    TAKE_WINDOW_SCREENSHOT },
  { true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE, BRIGHTNESS_DOWN },
  { true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_ALT_DOWN, KEYBOARD_BRIGHTNESS_DOWN },
  { true, ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE, BRIGHTNESS_UP },
  { true, ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN, KEYBOARD_BRIGHTNESS_UP },
  { true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    MAGNIFY_SCREEN_ZOOM_OUT},
  { true, ui::VKEY_BRIGHTNESS_UP, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    MAGNIFY_SCREEN_ZOOM_IN},
  { true, ui::VKEY_L, ui::EF_COMMAND_DOWN, LOCK_SCREEN },
  // The lock key on Chrome OS keyboards produces F13 scancodes.
  { true, ui::VKEY_F13, ui::EF_NONE, LOCK_PRESSED },
  { false, ui::VKEY_F13, ui::EF_NONE, LOCK_RELEASED },
  // Generic keyboards can use VKEY_SLEEP to mimic ChromeOS keyboard's lock key.
  { true, ui::VKEY_SLEEP, ui::EF_NONE, LOCK_PRESSED },
  { false, ui::VKEY_SLEEP, ui::EF_NONE, LOCK_RELEASED },
  { true, ui::VKEY_POWER, ui::EF_NONE, POWER_PRESSED },
  { false, ui::VKEY_POWER, ui::EF_NONE, POWER_RELEASED },
  { true, ui::VKEY_M, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
    OPEN_FILE_MANAGER },
  { true, ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN, OPEN_GET_HELP },
  { true, ui::VKEY_OEM_2, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    OPEN_GET_HELP },
  { true, ui::VKEY_T, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, OPEN_CROSH },
  { true, ui::VKEY_G, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    DISABLE_GPU_WATCHDOG },
  { true, ui::VKEY_I, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    TOUCH_HUD_MODE_CHANGE },
  { true, ui::VKEY_I, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
    TOUCH_HUD_CLEAR },
  { true, ui::VKEY_P, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    TOUCH_HUD_PROJECTION_TOGGLE },
  // Accessibility: Spoken feedback shortcuts. The first one is to toggle
  // spoken feedback on or off. The others are only valid when
  // spoken feedback is enabled.
  { true, ui::VKEY_Z, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    TOGGLE_SPOKEN_FEEDBACK },
  { true, ui::VKEY_OEM_COMMA, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    SWITCH_TO_PREVIOUS_USER },
  { true, ui::VKEY_OEM_PERIOD, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    SWITCH_TO_NEXT_USER },
  // Turning the TouchView maximizing mode on via hotkey for the time being.
  // TODO(skuhne): Remove once the test isn't needed anymore.
  { true, ui::VKEY_8, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
    TOGGLE_TOUCH_VIEW_TESTING },
  // Single shift release turns off caps lock.
  { false, ui::VKEY_LSHIFT, ui::EF_NONE, DISABLE_CAPS_LOCK },
  { false, ui::VKEY_SHIFT, ui::EF_NONE, DISABLE_CAPS_LOCK },
  { false, ui::VKEY_RSHIFT, ui::EF_NONE, DISABLE_CAPS_LOCK },
  { false, ui::VKEY_LWIN, ui::EF_ALT_DOWN, TOGGLE_CAPS_LOCK },
  { true, ui::VKEY_VOLUME_MUTE, ui::EF_NONE, VOLUME_MUTE },
  { true, ui::VKEY_VOLUME_DOWN, ui::EF_NONE, VOLUME_DOWN },
  { true, ui::VKEY_VOLUME_UP, ui::EF_NONE, VOLUME_UP },
  { true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN, SHOW_TASK_MANAGER },
  { true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, NEXT_IME },
#else
  // This key has been deprecated on CrOS. It is instead included below in the
  // |kDeprecatedAccelerators|, and above in the CrOS accelerators as
  // Search+Esc.
  { true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN, SHOW_TASK_MANAGER },
  // The below keys have been deprecated on ChromeOS and were replaced above by
  // Ctrl+Shift+Space. crbug.com/535004.
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
#endif  // defined(OS_CHROMEOS)
  { true, ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, OPEN_FEEDBACK_PAGE },
#if !defined(OS_WIN)
  { true, ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, EXIT },
#endif
  { true, ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    NEW_INCOGNITO_WINDOW },
  { true, ui::VKEY_N, ui::EF_CONTROL_DOWN, NEW_WINDOW },
  { true, ui::VKEY_T, ui::EF_CONTROL_DOWN, NEW_TAB },
  { true, ui::VKEY_OEM_MINUS,
    ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, SCALE_UI_UP },
  { true, ui::VKEY_OEM_PLUS,
    ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, SCALE_UI_DOWN },
  { true, ui::VKEY_0,
    ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, SCALE_UI_RESET },
  { true, ui::VKEY_BROWSER_REFRESH,
    ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, ROTATE_SCREEN },
  { true, ui::VKEY_BROWSER_REFRESH,
    ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    ROTATE_WINDOW },
  { true, ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, RESTORE_TAB },
  { true, ui::VKEY_PRINT, ui::EF_NONE, TAKE_SCREENSHOT },
  // On Chrome OS, Search key is mapped to LWIN. The Search key binding should
  // act on release instead of press when using Search as a modifier key for
  // extended keyboard shortcuts.
  { false, ui::VKEY_LWIN, ui::EF_NONE, TOGGLE_APP_LIST },
  { true, ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_NONE, TOGGLE_FULLSCREEN },
  { true, ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_SHIFT_DOWN, TOGGLE_FULLSCREEN },
  { true, ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, FOCUS_SHELF },
  { true, ui::VKEY_HELP, ui::EF_NONE, SHOW_KEYBOARD_OVERLAY },
  { true, ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    SHOW_KEYBOARD_OVERLAY },
  { true, ui::VKEY_OEM_2,
    ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
    SHOW_KEYBOARD_OVERLAY },
  { true, ui::VKEY_F14, ui::EF_NONE, SHOW_KEYBOARD_OVERLAY },
  { true, ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
    SHOW_MESSAGE_CENTER_BUBBLE },
  { true, ui::VKEY_S, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
    SHOW_SYSTEM_TRAY_BUBBLE },
  { true, ui::VKEY_1, ui::EF_ALT_DOWN, LAUNCH_APP_0 },
  { true, ui::VKEY_2, ui::EF_ALT_DOWN, LAUNCH_APP_1 },
  { true, ui::VKEY_3, ui::EF_ALT_DOWN, LAUNCH_APP_2 },
  { true, ui::VKEY_4, ui::EF_ALT_DOWN, LAUNCH_APP_3 },
  { true, ui::VKEY_5, ui::EF_ALT_DOWN, LAUNCH_APP_4 },
  { true, ui::VKEY_6, ui::EF_ALT_DOWN, LAUNCH_APP_5 },
  { true, ui::VKEY_7, ui::EF_ALT_DOWN, LAUNCH_APP_6 },
  { true, ui::VKEY_8, ui::EF_ALT_DOWN, LAUNCH_APP_7 },
  { true, ui::VKEY_9, ui::EF_ALT_DOWN, LAUNCH_LAST_APP },

  // Window management shortcuts.
  { true, ui::VKEY_OEM_4, ui::EF_ALT_DOWN, WINDOW_CYCLE_SNAP_DOCK_LEFT },
  { true, ui::VKEY_OEM_6, ui::EF_ALT_DOWN, WINDOW_CYCLE_SNAP_DOCK_RIGHT },
  { true, ui::VKEY_OEM_MINUS, ui::EF_ALT_DOWN, WINDOW_MINIMIZE },
  { true, ui::VKEY_OEM_PLUS, ui::EF_ALT_DOWN, TOGGLE_MAXIMIZED },
  { true, ui::VKEY_OEM_PLUS, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
    WINDOW_POSITION_CENTER },
  { true, ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN, FOCUS_NEXT_PANE },
  { true, ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN, FOCUS_PREVIOUS_PANE },

  // Media Player shortcuts.
  { true, ui::VKEY_MEDIA_NEXT_TRACK, ui::EF_NONE, MEDIA_NEXT_TRACK},
  { true, ui::VKEY_MEDIA_PLAY_PAUSE, ui::EF_NONE, MEDIA_PLAY_PAUSE},
  { true, ui::VKEY_MEDIA_PREV_TRACK, ui::EF_NONE, MEDIA_PREV_TRACK},

  // Debugging shortcuts that need to be available to end-users in
  // release builds.
  { true, ui::VKEY_U, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
    PRINT_UI_HIERARCHIES },

  // TODO(yusukes): Handle VKEY_MEDIA_STOP, and
  // VKEY_MEDIA_LAUNCH_MAIL.
};

const size_t kAcceleratorDataLength = arraysize(kAcceleratorData);

// Instructions for how to deprecate and replace an Accelerator:
//
// 1- Replace the old deprecated accelerator from the above list with the new
//    accelerator that will take its place.
// 2- Add an entry for it in the following |kDeprecatedAccelerators| list.
// 3- Add another entry in the |kDeprecatedAcceleratorsData|.
// 4- That entry should contain the following:
//    - The action that the deprecated accelerator maps to.
//    - Define a histogram for this action in |histograms.xml| in the form
//      "Ash.Accelerators.Deprecated.{ActionName}" and include the name of this
//      histogram in this entry. This name will be used as the ID of the
//      notification to be shown to the user. This is to prevent duplication of
//      same notification.
//    - The ID of the localized notification message to give the users telling
//      them about the deprecation (Add one in |ash_chromeos_strings.grdp|.
//      Search for the comment <!-- Deprecated Accelerators Messages -->).
//    - The IDs of the localized old and new shortcut text to be used to fill
//      the notification text. Also found in |ash_chromeos_strings.grdp|.
//    - {true or false} whether the deprecated accelerator is still enabled (we
//      don't disable a deprecated accelerator abruptly).
// 5- Don't forget to update the keyboard overlay. Find 'shortcut' in the file
//    keyboard_overlay_data.js.
#if defined(OS_CHROMEOS)

const AcceleratorData kDeprecatedAccelerators[] = {
  { true, ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, LOCK_SCREEN },
  { true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN, SHOW_TASK_MANAGER },
  { false, ui::VKEY_LMENU, ui::EF_SHIFT_DOWN, NEXT_IME },
  { false, ui::VKEY_MENU, ui::EF_SHIFT_DOWN, NEXT_IME },
  { false, ui::VKEY_RMENU, ui::EF_SHIFT_DOWN, NEXT_IME },
  { false, ui::VKEY_LSHIFT, ui::EF_ALT_DOWN, NEXT_IME },
  { false, ui::VKEY_SHIFT, ui::EF_ALT_DOWN, NEXT_IME },
  { false, ui::VKEY_RSHIFT, ui::EF_ALT_DOWN, NEXT_IME },
};

const size_t kDeprecatedAcceleratorsLength = arraysize(kDeprecatedAccelerators);

const DeprecatedAcceleratorData kDeprecatedAcceleratorsData[] = {
  {
    LOCK_SCREEN,
    "Ash.Accelerators.Deprecated.LockScreen",
    IDS_DEPRECATED_LOCK_SCREEN_MSG,
    IDS_SHORTCUT_LOCK_SCREEN_OLD,
    IDS_SHORTCUT_LOCK_SCREEN_NEW,
    true
  },
  {
    SHOW_TASK_MANAGER,
    "Ash.Accelerators.Deprecated.ShowTaskManager",
    IDS_DEPRECATED_SHOW_TASK_MANAGER_MSG,
    IDS_SHORTCUT_TASK_MANAGER_OLD,
    IDS_SHORTCUT_TASK_MANAGER_NEW,
    true
  },
  {
    NEXT_IME,
    "Ash.Accelerators.Deprecated.NextIME",
    IDS_DEPRECATED_NEXT_IME_MSG,
    IDS_SHORTCUT_NEXT_IME_OLD,
    IDS_SHORTCUT_NEXT_IME_NEW,
    true
  }
};

const size_t kDeprecatedAcceleratorsDataLength =
    arraysize(kDeprecatedAcceleratorsData);

#endif  // defined(OS_CHROMEOS)

const AcceleratorData kDebugAcceleratorData[] = {
#if defined(OS_CHROMEOS)
    // Extra shortcut for debug build to control magnifier on linux desktop.
    {true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_CONTROL_DOWN,
     MAGNIFY_SCREEN_ZOOM_OUT},
    {true, ui::VKEY_BRIGHTNESS_UP, ui::EF_CONTROL_DOWN, MAGNIFY_SCREEN_ZOOM_IN},
    // Extra shortcuts to lock the screen on linux desktop.
    {true, ui::VKEY_L, ui::EF_ALT_DOWN, LOCK_SCREEN},
    {true, ui::VKEY_POWER, ui::EF_SHIFT_DOWN, LOCK_PRESSED},
    {false, ui::VKEY_POWER, ui::EF_SHIFT_DOWN, LOCK_RELEASED},
    {true, ui::VKEY_D, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     DEBUG_ADD_REMOVE_DISPLAY},
    {true, ui::VKEY_J, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     DEBUG_TOGGLE_UNIFIED_DESKTOP},
    {true, ui::VKEY_M, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     TOGGLE_MIRROR_MODE},
    {true, ui::VKEY_P, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     DEBUG_TOGGLE_TOUCH_PAD},
    {true, ui::VKEY_T, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     DEBUG_TOGGLE_TOUCH_SCREEN},
    {true, ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, TOGGLE_WIFI},
    // Extra shortcut for display swapping as alt-f4 is taken on linux desktop.
    {true, ui::VKEY_S, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     SWAP_PRIMARY_DISPLAY},
#endif
    // Extra shortcut to rotate/scale up/down the screen on linux desktop.
    {true, ui::VKEY_R,
     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, ROTATE_SCREEN},
    // For testing on systems where Alt-Tab is already mapped.
    {true, ui::VKEY_W, ui::EF_ALT_DOWN, CYCLE_FORWARD_MRU},
    {true, ui::VKEY_F11, ui::EF_CONTROL_DOWN,
     DEBUG_TOGGLE_ROOT_WINDOW_FULL_SCREEN},
    {true, ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
    {true, ui::VKEY_B, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     DEBUG_TOGGLE_DESKTOP_BACKGROUND_MODE},
    {true, ui::VKEY_F, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     TOGGLE_FULLSCREEN},
    {true, ui::VKEY_L,
     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     DEBUG_PRINT_LAYER_HIERARCHY},
    {true, ui::VKEY_V,
     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     DEBUG_PRINT_VIEW_HIERARCHY},
    {true, ui::VKEY_W,
     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     DEBUG_PRINT_WINDOW_HIERARCHY},
    {true, ui::VKEY_D,
     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     DEBUG_TOGGLE_DEVICE_SCALE_FACTOR},
    {true, ui::VKEY_B,
     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     DEBUG_TOGGLE_SHOW_DEBUG_BORDERS},
    {true, ui::VKEY_F,
     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     DEBUG_TOGGLE_SHOW_FPS_COUNTER},
    {true, ui::VKEY_P,
     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     DEBUG_TOGGLE_SHOW_PAINT_RECTS},
};

const size_t kDebugAcceleratorDataLength = arraysize(kDebugAcceleratorData);

const AcceleratorAction kPreferredActions[] = {
  // Window cycling accelerators.
  CYCLE_BACKWARD_MRU,  // Shift+Alt+Tab
  CYCLE_FORWARD_MRU,  // Alt+Tab
};

const size_t kPreferredActionsLength = arraysize(kPreferredActions);

const AcceleratorAction kReservedActions[] = {
#if defined(OS_CHROMEOS)
  POWER_PRESSED,
  POWER_RELEASED,
#else
  DUMMY_FOR_RESERVED,
#endif
};

const size_t kReservedActionsLength = arraysize(kReservedActions);

const AcceleratorAction kActionsAllowedAtLoginOrLockScreen[] = {
  DEBUG_PRINT_LAYER_HIERARCHY,
  DEBUG_PRINT_VIEW_HIERARCHY,
  DEBUG_PRINT_WINDOW_HIERARCHY,
  MAGNIFY_SCREEN_ZOOM_IN,  // Control+F7
  MAGNIFY_SCREEN_ZOOM_OUT,  // Control+F6
  NEXT_IME,
  PREVIOUS_IME,
  PRINT_UI_HIERARCHIES,
  ROTATE_SCREEN,
  SCALE_UI_UP,
  SCALE_UI_DOWN,
  SCALE_UI_RESET,
  SHOW_SYSTEM_TRAY_BUBBLE,
  SWITCH_IME,  // Switch to another IME depending on the accelerator.
  TAKE_WINDOW_SCREENSHOT,
  TAKE_PARTIAL_SCREENSHOT,
  TAKE_SCREENSHOT,
#if defined(OS_CHROMEOS)
  BRIGHTNESS_DOWN,
  BRIGHTNESS_UP,
  DEBUG_ADD_REMOVE_DISPLAY,
  DEBUG_TOGGLE_TOUCH_PAD,
  DEBUG_TOGGLE_TOUCH_SCREEN,
  DISABLE_CAPS_LOCK,
  DISABLE_GPU_WATCHDOG,
  KEYBOARD_BRIGHTNESS_DOWN,
  KEYBOARD_BRIGHTNESS_UP,
  TOGGLE_CAPS_LOCK,
  TOGGLE_SPOKEN_FEEDBACK,
  TOGGLE_TOUCH_VIEW_TESTING,
  TOGGLE_MIRROR_MODE,
  TOGGLE_WIFI,
  TOUCH_HUD_CLEAR,
  VOLUME_DOWN,
  VOLUME_MUTE,
  VOLUME_UP,
#endif
#if defined(OS_CHROMEOS) && !defined(NDEBUG)
  POWER_PRESSED,
  POWER_RELEASED,
#endif  // defined(OS_CHROMEOS)
};

const size_t kActionsAllowedAtLoginOrLockScreenLength =
    arraysize(kActionsAllowedAtLoginOrLockScreen);

const AcceleratorAction kActionsAllowedAtLockScreen[] = {
  EXIT,
};

const size_t kActionsAllowedAtLockScreenLength =
    arraysize(kActionsAllowedAtLockScreen);

const AcceleratorAction kActionsAllowedAtModalWindow[] = {
  EXIT,
  MAGNIFY_SCREEN_ZOOM_IN,
  MAGNIFY_SCREEN_ZOOM_OUT,
  MEDIA_NEXT_TRACK,
  MEDIA_PLAY_PAUSE,
  MEDIA_PREV_TRACK,
  NEXT_IME,
  OPEN_FEEDBACK_PAGE,
  PREVIOUS_IME,
  PRINT_UI_HIERARCHIES,
  ROTATE_SCREEN,
  SCALE_UI_UP,
  SCALE_UI_DOWN,
  SCALE_UI_RESET,
  SHOW_KEYBOARD_OVERLAY,
  SWITCH_IME,
  TAKE_WINDOW_SCREENSHOT,
  TAKE_PARTIAL_SCREENSHOT,
  TAKE_SCREENSHOT,
#if defined(OS_CHROMEOS)
  BRIGHTNESS_DOWN,
  BRIGHTNESS_UP,
  DEBUG_ADD_REMOVE_DISPLAY,
  DEBUG_TOGGLE_TOUCH_PAD,
  DEBUG_TOGGLE_TOUCH_SCREEN,
  DISABLE_CAPS_LOCK,
  KEYBOARD_BRIGHTNESS_DOWN,
  KEYBOARD_BRIGHTNESS_UP,
  LOCK_SCREEN,
  POWER_PRESSED,
  POWER_RELEASED,
  SWAP_PRIMARY_DISPLAY,
  TOGGLE_CAPS_LOCK,
  TOGGLE_MIRROR_MODE,
  TOGGLE_SPOKEN_FEEDBACK,
  TOGGLE_WIFI,
  VOLUME_DOWN,
  VOLUME_MUTE,
  VOLUME_UP,
#endif
};

const size_t kActionsAllowedAtModalWindowLength =
    arraysize(kActionsAllowedAtModalWindow);

const AcceleratorAction kNonrepeatableActions[] = {
    // TODO(mazda): Add other actions which should not be repeated.
    CYCLE_BACKWARD_MRU,
    CYCLE_FORWARD_MRU,
    EXIT,
    OPEN_FEEDBACK_PAGE,
    PRINT_UI_HIERARCHIES,  // Don't fill the logs if the key is held down.
    ROTATE_SCREEN,
    ROTATE_WINDOW,
    SCALE_UI_UP,
    SCALE_UI_DOWN,
    SCALE_UI_RESET,
    TAKE_WINDOW_SCREENSHOT,
    TAKE_PARTIAL_SCREENSHOT,
    TAKE_SCREENSHOT,
    TOGGLE_FULLSCREEN,
    TOGGLE_MAXIMIZED,
    TOGGLE_OVERVIEW,
    WINDOW_MINIMIZE,
#if defined(OS_CHROMEOS)
    DEBUG_TOGGLE_TOUCH_PAD,
    DEBUG_TOGGLE_TOUCH_SCREEN,
    LOCK_SCREEN,
#endif
};

const size_t kNonrepeatableActionsLength =
    arraysize(kNonrepeatableActions);

const AcceleratorAction kActionsAllowedInAppMode[] = {
  DEBUG_PRINT_LAYER_HIERARCHY,
  DEBUG_PRINT_VIEW_HIERARCHY,
  DEBUG_PRINT_WINDOW_HIERARCHY,
  MAGNIFY_SCREEN_ZOOM_IN,  // Control+F7
  MAGNIFY_SCREEN_ZOOM_OUT,  // Control+F6
  MEDIA_NEXT_TRACK,
  MEDIA_PLAY_PAUSE,
  MEDIA_PREV_TRACK,
  NEXT_IME,
  PREVIOUS_IME,
  PRINT_UI_HIERARCHIES,
  ROTATE_SCREEN,
  SCALE_UI_DOWN,
  SCALE_UI_RESET,
  SCALE_UI_UP,
  SWITCH_IME,  // Switch to another IME depending on the accelerator.
#if defined(OS_CHROMEOS)
  BRIGHTNESS_DOWN,
  BRIGHTNESS_UP,
  DEBUG_ADD_REMOVE_DISPLAY,
  DEBUG_TOGGLE_TOUCH_PAD,
  DEBUG_TOGGLE_TOUCH_SCREEN,
  DISABLE_CAPS_LOCK,
  DISABLE_GPU_WATCHDOG,
  KEYBOARD_BRIGHTNESS_DOWN,
  KEYBOARD_BRIGHTNESS_UP,
  POWER_PRESSED,
  POWER_RELEASED,
  SWAP_PRIMARY_DISPLAY,
  TOGGLE_CAPS_LOCK,
  TOGGLE_MIRROR_MODE,
  TOGGLE_SPOKEN_FEEDBACK,
  TOGGLE_WIFI,
  TOUCH_HUD_CLEAR,
  VOLUME_DOWN,
  VOLUME_MUTE,
  VOLUME_UP,
#endif  // defined(OS_CHROMEOS)
};

const size_t kActionsAllowedInAppModeLength =
    arraysize(kActionsAllowedInAppMode);

const AcceleratorAction kActionsNeedingWindow[] = {
    CYCLE_BACKWARD_MRU,
    CYCLE_FORWARD_MRU,
    TOGGLE_OVERVIEW,
    WINDOW_CYCLE_SNAP_DOCK_LEFT,
    WINDOW_CYCLE_SNAP_DOCK_RIGHT,
    WINDOW_MINIMIZE,
    TOGGLE_FULLSCREEN,
    TOGGLE_MAXIMIZED,
    WINDOW_POSITION_CENTER,
    ROTATE_WINDOW,
};

const size_t kActionsNeedingWindowLength = arraysize(kActionsNeedingWindow);

const AcceleratorAction kActionsKeepingMenuOpen[] = {
    MEDIA_NEXT_TRACK,
    MEDIA_PLAY_PAUSE,
    MEDIA_PREV_TRACK,
    NEXT_IME,
    PREVIOUS_IME,
    PRINT_UI_HIERARCHIES,
    SWITCH_IME,
    TAKE_WINDOW_SCREENSHOT,
    TAKE_PARTIAL_SCREENSHOT,
    TAKE_SCREENSHOT,
#if defined(OS_CHROMEOS)
    BRIGHTNESS_DOWN,
    BRIGHTNESS_UP,
    DEBUG_TOGGLE_TOUCH_PAD,
    DEBUG_TOGGLE_TOUCH_SCREEN,
    DISABLE_CAPS_LOCK,
    KEYBOARD_BRIGHTNESS_DOWN,
    KEYBOARD_BRIGHTNESS_UP,
    TOGGLE_CAPS_LOCK,
    TOGGLE_SPOKEN_FEEDBACK,
    TOGGLE_WIFI,
    VOLUME_DOWN,
    VOLUME_MUTE,
    VOLUME_UP,
#endif  // defined(OS_CHROMEOS)
};

const size_t kActionsKeepingMenuOpenLength = arraysize(kActionsKeepingMenuOpen);

}  // namespace ash
