// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_XKEYBOARD_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_XKEYBOARD_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace chromeos {
namespace input_method {

struct AutoRepeatRate {
  AutoRepeatRate() : initial_delay_in_ms(0), repeat_interval_in_ms(0) {}
  unsigned int initial_delay_in_ms;
  unsigned int repeat_interval_in_ms;
};

enum ModifierLockStatus {
  kDisableLock = 0,
  kEnableLock,
  kDontChange,
};

enum ModifierKey {
  kSearchKey = 0,  // Customizable.
  kControlKey,  // Customizable.
  kAltKey,  // Customizable.
  kVoidKey,
  kCapsLockKey,
  // IMPORTANT: You should update kCustomizableKeys[] in .cc file, if you
  // add a customizable key.
  kNumModifierKeys,
};

class InputMethodUtil;

class XKeyboard {
 public:
  virtual ~XKeyboard() {}

  // Sets the current keyboard layout to |layout_name|. This function does not
  // change the current mapping of the modifier keys. Returns true on success.
  virtual bool SetCurrentKeyboardLayoutByName(
      const std::string& layout_name) = 0;

  // Sets the current keyboard layout again. We have to call the function every
  // time when "XI_HierarchyChanged" XInput2 event is sent to Chrome. See
  // xinput_hierarchy_changed_event_listener.h for details.
  virtual bool ReapplyCurrentKeyboardLayout() = 0;

  // Updates keyboard LEDs on all keyboards.
  // XKB asymmetrically propagates keyboard modifier indicator state changes to
  // slave keyboards. If the state change is initiated from a client to the
  // "core/master keyboard", XKB changes global state and pushes an indication
  // change down to all keyboards. If the state change is initiated by one slave
  // (physical) keyboard, it changes global state but only pushes an indicator
  // state change down to that one keyboard.
  // This function changes LEDs on all keyboards by explicitly updating the
  // core/master keyboard.
  virtual void ReapplyCurrentModifierLockStatus() = 0;

  // Sets the Caps Lock and Num Lock status. Do not call the function from
  // non-UI threads.
  virtual void SetLockedModifiers(ModifierLockStatus new_caps_lock_status,
                                  ModifierLockStatus new_num_lock_status) = 0;

  // Sets the num lock status to |enable_num_lock|. Do not call the function
  // from non-UI threads.
  virtual void SetNumLockEnabled(bool enable_num_lock) = 0;

  // Sets the caps lock status to |enable_caps_lock|. Do not call the function
  // from non-UI threads.
  virtual void SetCapsLockEnabled(bool enable_caps_lock) = 0;

  // Returns true if num lock is enabled. Do not call the function from non-UI
  // threads.
  virtual bool NumLockIsEnabled() = 0;

  // Returns true if caps lock is enabled. Do not call the function from non-UI
  // threads.
  virtual bool CapsLockIsEnabled() = 0;

  // Returns a mask (e.g. 1U<<4) for Num Lock. On error, returns 0. Do not call
  // the function from non-UI threads.
  // TODO(yusukes): Move this and webdriver::GetXModifierMask() functions in
  // chrome/test/webdriver/keycode_text_conversion_x.cc to ui/base/x/x11_util.
  // The two functions are almost the same.
  virtual unsigned int GetNumLockMask() = 0;

  // Set true on |out_caps_lock_enabled| if Caps Lock is enabled. Set true on
  // |out_num_lock_enabled| if Num Lock is enabled. Both out parameters can be
  // NULL. Do not call the function from non-UI threads.
  virtual void GetLockedModifiers(bool* out_caps_lock_enabled,
                                  bool* out_num_lock_enabled) = 0;

  // Turns on and off the auto-repeat of the keyboard. Returns true on success.
  // Do not call the function from non-UI threads.
  // TODO(yusukes): Make this function non-static so we can mock it.
  static bool SetAutoRepeatEnabled(bool enabled);

  // Sets the auto-repeat rate of the keyboard, initial delay in ms, and repeat
  // interval in ms.  Returns true on success. Do not call the function from
  // non-UI threads.
  // TODO(yusukes): Make this function non-static so we can mock it.
  static bool SetAutoRepeatRate(const AutoRepeatRate& rate);

  // Returns true if auto repeat is enabled. This function is protected: for
  // testability.
  static bool GetAutoRepeatEnabledForTesting();

  // On success, set current auto repeat rate on |out_rate| and returns true.
  // Returns false otherwise. This function is protected: for testability.
  static bool GetAutoRepeatRateForTesting(AutoRepeatRate* out_rate);

  // Returns false if |layout_name| contains a bad character.
  static bool CheckLayoutNameForTesting(const std::string& layout_name);

  // Note: At this moment, classes other than InputMethodManager should not
  // instantiate the XKeyboard class.
  static XKeyboard* Create();
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_XKEYBOARD_H_
