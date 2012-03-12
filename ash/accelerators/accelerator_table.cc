// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_table.h"

#include "base/basictypes.h"

namespace ash {

const AcceleratorData kAcceleratorData[] = {
  // Accelerators that should be processed before a key is sent to an IME.
  { ui::ET_KEY_RELEASED, ui::VKEY_MENU, true, false, true, NEXT_IME },
  { ui::ET_KEY_RELEASED, ui::VKEY_SHIFT, true, false, true, NEXT_IME },
  { ui::ET_KEY_PRESSED, ui::VKEY_SPACE, false, true, false, PREVIOUS_IME },
  // Shortcuts for Japanese IME.
  { ui::ET_KEY_PRESSED, ui::VKEY_CONVERT, false, false, false, SWITCH_IME },
  { ui::ET_KEY_PRESSED, ui::VKEY_NONCONVERT, false, false, false, SWITCH_IME },
  { ui::ET_KEY_PRESSED, ui::VKEY_DBE_SBCSCHAR, false, false, false,
    SWITCH_IME },
  { ui::ET_KEY_PRESSED, ui::VKEY_DBE_DBCSCHAR, false, false, false,
    SWITCH_IME },
  // Shortcuts for Koren IME.
  { ui::ET_KEY_PRESSED, ui::VKEY_HANGUL, false, false, false, SWITCH_IME },
  { ui::ET_KEY_PRESSED, ui::VKEY_SPACE, true, false, false, SWITCH_IME },

  // Accelerators that should be processed after a key is sent to an IME.
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_TAB, false, false, true,
    CYCLE_FORWARD },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_TAB, true, false, true,
    CYCLE_BACKWARD },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F5, false, false, false,
    CYCLE_FORWARD },
#if defined(OS_CHROMEOS)
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_BRIGHTNESS_DOWN, false, false, false,
    BRIGHTNESS_DOWN },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_BRIGHTNESS_UP, false, false, false,
    BRIGHTNESS_UP },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_L, true, true, false, LOCK_SCREEN },
#endif
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_Q, true, true, false, EXIT },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F5, true, false, false,
    CYCLE_BACKWARD },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F5, false, true, false,
    TAKE_SCREENSHOT },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F5, true, true, false,
    TAKE_PARTIAL_SCREENSHOT },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_PRINT, false, false, false,
    TAKE_SCREENSHOT },
  // On Chrome OS, Search key is mapped to LWIN.
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_LWIN, false, true, false,
    TOGGLE_APP_LIST },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_LWIN, true, false, false,
    TOGGLE_CAPS_LOCK },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F6, false, false, false,
    BRIGHTNESS_DOWN },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F7, false, false, false,
    BRIGHTNESS_UP },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F8, false, false, false,
    VOLUME_MUTE },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_VOLUME_MUTE, false, false, false,
    VOLUME_MUTE },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F9, false, false, false,
    VOLUME_DOWN },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_VOLUME_DOWN, false, false, false,
    VOLUME_DOWN },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F10, false, false, false, VOLUME_UP },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_VOLUME_UP, false, false, false,
    VOLUME_UP },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F1, true, true, false, SHOW_OAK },
#if !defined(NDEBUG)
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_HOME, false, true, false,
    ROTATE_SCREEN },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_B, false, true, true,
    TOGGLE_DESKTOP_BACKGROUND_MODE },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F11, false, true, false,
    TOGGLE_ROOT_WINDOW_FULL_SCREEN },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_L, false, false, true,
    PRINT_LAYER_HIERARCHY },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_L, true, false, true,
    PRINT_WINDOW_HIERARCHY },
  // For testing on systems where Alt-Tab is already mapped.
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_W, false, false, true,
    CYCLE_FORWARD },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_W, true, false, true,
    CYCLE_BACKWARD },
#endif
};

const size_t kAcceleratorDataLength = arraysize(kAcceleratorData);

}  // namespace ash
