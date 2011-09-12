// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/hotkey_manager.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/stl_util.h"

#include <X11/X.h>  // ShiftMask, ControlMask, etc.
#include <X11/Xlib.h>  // KeySym, XLookupString.
#include <X11/Xutil.h>  // XK_* macros.

namespace {

uint32 KeySymToModifier(KeySym keysym) {
  switch (keysym) {
    case XK_Control_L:
      return ControlMask;
    case XK_Control_R:
      return ControlMask;
    case XK_Shift_L:
      return ShiftMask;
    case XK_Shift_R:
      return ShiftMask;
    case XK_Alt_L:
      return Mod1Mask;
    case XK_Alt_R:
      return Mod1Mask;
    case XK_Meta_L:
      return Mod1Mask;
    case XK_Meta_R:
      return Mod1Mask;
    case XK_Super_L:
      return Mod2Mask;
    case XK_Super_R:
      return Mod2Mask;
    case XK_Hyper_L:
      return Mod3Mask;
    case XK_Hyper_R:
      return Mod3Mask;
    default:
      break;
  }
  return 0;
}

}  // namespace

namespace chromeos {
namespace input_method {

const int HotkeyManager::kNoEvent = -1;
const int HotkeyManager::kFiltered = -2;
const uint32 HotkeyManager::kKeyReleaseMask = 1U << 31;

// Make sure kKeyReleaseMask is not the same value as XXXMask in X11/X.h.
COMPILE_ASSERT(ControlMask != (1U << 31), CheckMaskValue);
COMPILE_ASSERT(ShiftMask != (1U << 31), CheckMaskValue);
COMPILE_ASSERT(LockMask != (1U << 31), CheckMaskValue);
COMPILE_ASSERT(Mod1Mask != (1U << 31), CheckMaskValue);
COMPILE_ASSERT(Mod2Mask != (1U << 31), CheckMaskValue);
COMPILE_ASSERT(Mod3Mask != (1U << 31), CheckMaskValue);
COMPILE_ASSERT(Mod4Mask != (1U << 31), CheckMaskValue);
COMPILE_ASSERT(Mod5Mask != (1U << 31), CheckMaskValue);

HotkeyManager::HotkeyManager()
  : previous_keysym_(NoSymbol),
    previous_modifiers_(0x0U),
    filter_release_events_(false) {
}

HotkeyManager::~HotkeyManager() {
}

void HotkeyManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HotkeyManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool HotkeyManager::AddHotkey(int event_id,
                              uint32 keysym,
                              uint32 modifiers,
                              bool trigger_on_key_press) {
  if (event_id < 0) {
    LOG(ERROR) << "Bad hotkey event id: " << event_id;
    return false;
  }
  modifiers = NormalizeModifiers(keysym, modifiers, trigger_on_key_press);
  return hotkeys_.insert(
      std::make_pair(std::make_pair(keysym, modifiers), event_id)).second;
}

bool HotkeyManager::RemoveHotkey(int event_id) {
  bool result = false;
  std::map<std::pair<uint32, uint32>, int>::iterator iter;
  for (iter = hotkeys_.begin(); iter != hotkeys_.end();) {
    if (iter->second == event_id) {
      hotkeys_.erase(iter++);
      result = true;
    } else {
      ++iter;
    }
  }
  return result;
}

bool HotkeyManager::FilterKeyEvent(const XEvent& key_event) {
  bool is_key_press = true;
  switch (key_event.type) {
    case KeyRelease:
      is_key_press = false;
      /* fall through */
    case KeyPress:
      break;
    default:
      LOG(ERROR) << "Unknown event: " << key_event.type;
      return false;
  }

  KeySym keysym = NoSymbol;
  ::XLookupString(
      const_cast<XKeyEvent*>(&key_event.xkey), NULL, 0, &keysym, NULL);

  const int event_id =
      FilterKeyEventInternal(keysym, key_event.xkey.state, is_key_press);
  if (event_id >= 0) {
    FOR_EACH_OBSERVER(Observer, observers_, HotkeyPressed(this, event_id));
    return true;  // The key event should be consumed since it's an IME hotkey.
  } else if (event_id == kFiltered) {
    return true;
  }
  return false;
}

uint32 HotkeyManager::NormalizeModifiers(
    uint32 keysym, uint32 modifiers, bool is_key_press) const {
  modifiers &= (ControlMask | ShiftMask | Mod1Mask | Mod2Mask | Mod3Mask);
  modifiers |= KeySymToModifier(keysym);
  if (!is_key_press) {
    modifiers |= kKeyReleaseMask;
  }
  return modifiers;
}

bool HotkeyManager::IsModifier(uint32 keysym) const {
  return KeySymToModifier(keysym) != 0;
}

int HotkeyManager::FilterKeyEventInternal(
    uint32 keysym, uint32 modifiers, bool is_key_press) {
  modifiers = NormalizeModifiers(keysym, modifiers, is_key_press);

  // This logic is the same as bus_input_context_filter_keyboard_shortcuts in
  // ibus-daemon.
  if (filter_release_events_) {
    if (modifiers & kKeyReleaseMask) {
      // This release event should not be passed to the application.
      return kFiltered;
    }
    filter_release_events_ = false;
  }

  int event_id = kNoEvent;
  // This logic is the same as ibus_hotkey_profile_filter_key_event in libibus.
  // See also http://crosbug.com/6225.
  do {
    if (modifiers & kKeyReleaseMask) {
      if (previous_modifiers_ & kKeyReleaseMask) {
        // The previous event has to be a key press event. This is necessary not
        // to switch IME when e.g. X is released after Shift+Alt+X is pressed.
        break;
      }
      if ((keysym != previous_keysym_) &&
          (!IsModifier(keysym) || !IsModifier(previous_keysym_))) {
        // This check is useful when e.g. Alt is released after Shift+Alt+X is
        // pressed, and the release event if X is intercepted by the window
        // manager. In this case, we should not switch IME, but the first check
        // does not work.
        // The IsModifier() checks are necessary for the 'press Alt, press
        // Shift, release Alt, release Shift' case to work.
        break;
      }
    }
    std::map<std::pair<uint32, uint32>, int>::const_iterator iter =
        hotkeys_.find(std::make_pair(keysym, modifiers));
    if (iter != hotkeys_.end()) {
      event_id = iter->second;
    }
  } while (false);

  // This logic is the same as bus_input_context_filter_keyboard_shortcuts in
  // ibus-daemon.
  previous_keysym_ = keysym;
  previous_modifiers_ = modifiers;
  // Start filtering release events if a hotkey is detected.
  filter_release_events_ = (event_id != kNoEvent);

  return event_id;
}

}  // namespace input_method
}  // namespace chromeos
