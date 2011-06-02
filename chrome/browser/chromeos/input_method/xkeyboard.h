// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_XKEYBOARD_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_XKEYBOARD_H_
#pragma once

#include <map>
#include <string>
#include <vector>

namespace chromeos {
namespace input_method {

struct AutoRepeatRate {
  AutoRepeatRate() : initial_delay_in_ms(0), repeat_interval_in_ms(0) {}
  unsigned int initial_delay_in_ms;
  unsigned int repeat_interval_in_ms;
};

enum ModifierKey {
  kSearchKey = 0,  // Customizable.
  kLeftControlKey,  // Customizable.
  kLeftAltKey,  // Customizable.
  kVoidKey,
  kCapsLockKey,
  // IMPORTANT: You should update kCustomizableKeys[] in .cc file, if you
  // add a customizable key.
  kNumModifierKeys,
};

struct ModifierKeyPair {
  ModifierKeyPair(ModifierKey in_original, ModifierKey in_replacement)
      : original(in_original), replacement(in_replacement) {}
  bool operator==(const ModifierKeyPair& rhs) const {
    // For CheckMap() in chromeos_keyboard_unittest.cc.
    return (rhs.original == original) && (rhs.replacement == replacement);
  }
  ModifierKey original;      // Replace the key with
  ModifierKey replacement;   // this key.
};
typedef std::vector<ModifierKeyPair> ModifierMap;

// Sets the current keyboard layout to |layout_name|. This function does not
// change the current mapping of the modifier keys. Returns true on success.
bool SetCurrentKeyboardLayoutByName(const std::string& layout_name);

// Remaps modifier keys. This function does not change the current keyboard
// layout. Returns true on success.
// Notice: For now, you can't remap Left Control and Left Alt keys to CapsLock.
bool RemapModifierKeys(const ModifierMap& modifier_map);

// Turns on and off the auto-repeat of the keyboard. Returns true on success.
bool SetAutoRepeatEnabled(bool enabled);

// Sets the auto-repeat rate of the keyboard, initial delay in ms, and repeat
// interval in ms.  Returns true on success.
bool SetAutoRepeatRate(const AutoRepeatRate& rate);

//
// WARNING: DO NOT USE FUNCTIONS/CLASSES/TYPEDEFS BELOW. They are only for
// unittests. See the definitions in chromeos_keyboard.cc for details.
//

// Converts |key| to a modifier key name which is used in
// /usr/share/X11/xkb/symbols/chromeos.
inline std::string ModifierKeyToString(ModifierKey key) {
  switch (key) {
    case kSearchKey:
      return "search";
    case kLeftControlKey:
      return "leftcontrol";
    case kLeftAltKey:
      return "leftalt";
    case kVoidKey:
      return "disabled";
    case kCapsLockKey:
      return "capslock";
    case kNumModifierKeys:
      break;
  }
  return "";
}

// Creates a full XKB layout name like
//   "gb(extd)+chromeos(leftcontrol_disabled_leftalt),us"
// from modifier key mapping and |layout_name|, such as "us", "us(dvorak)", and
// "gb(extd)". Returns an empty string on error.
std::string CreateFullXkbLayoutName(const std::string& layout_name,
                                    const ModifierMap& modifire_map);

// Returns true if caps lock is enabled.
// ONLY FOR UNIT TEST. DO NOT USE THIS FUNCTION.
bool CapsLockIsEnabled();

// Sets the caps lock status to |enable_caps_lock|.
void SetCapsLockEnabled(bool enabled);

// Returns true if |key| is in |modifier_map| as replacement.
bool ContainsModifierKeyAsReplacement(
    const ModifierMap& modifier_map, ModifierKey key);

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_XKEYBOARD_H_
