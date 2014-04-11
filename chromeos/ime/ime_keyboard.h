// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_IME_KEYBOARD_H_
#define CHROMEOS_IME_IME_KEYBOARD_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace input_method {

struct AutoRepeatRate {
  AutoRepeatRate() : initial_delay_in_ms(0), repeat_interval_in_ms(0) {}
  unsigned int initial_delay_in_ms;
  unsigned int repeat_interval_in_ms;
};

enum ModifierKey {
  kSearchKey = 0,  // Customizable.
  kControlKey,  // Customizable.
  kAltKey,  // Customizable.
  kVoidKey,
  kCapsLockKey,
  kEscapeKey,
  // IMPORTANT: You should update kCustomizableKeys[] in .cc file, if you
  // add a customizable key.
  kNumModifierKeys,
};

class InputMethodUtil;

class CHROMEOS_EXPORT ImeKeyboard {
 public:
  class Observer {
   public:
    // Called when the caps lock state has changed.
    virtual void OnCapsLockChanged(bool enabled) = 0;
  };

  virtual ~ImeKeyboard() {}

  // Adds/removes observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

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

  // Disables the num lock.
  virtual void DisableNumLock() = 0;

  // Sets the caps lock status to |enable_caps_lock|. Do not call the function
  // from non-UI threads.
  virtual void SetCapsLockEnabled(bool enable_caps_lock) = 0;

  // Returns true if caps lock is enabled. Do not call the function from non-UI
  // threads.
  virtual bool CapsLockIsEnabled() = 0;

  // Returns true if the current layout supports ISO Level 5 shift.
  virtual bool IsISOLevel5ShiftAvailable() const = 0;

  // Returns true if the current layout supports alt gr.
  virtual bool IsAltGrAvailable() const = 0;

  // Turns on and off the auto-repeat of the keyboard. Returns true on success.
  // Do not call the function from non-UI threads.
  virtual bool SetAutoRepeatEnabled(bool enabled) = 0;

  // Sets the auto-repeat rate of the keyboard, initial delay in ms, and repeat
  // interval in ms.  Returns true on success. Do not call the function from
  // non-UI threads.
  virtual bool SetAutoRepeatRate(const AutoRepeatRate& rate) = 0;

  // Returns true if auto repeat is enabled. This function is protected: for
  // testability.
  static CHROMEOS_EXPORT bool GetAutoRepeatEnabledForTesting();

  // On success, set current auto repeat rate on |out_rate| and returns true.
  // Returns false otherwise. This function is protected: for testability.
  static CHROMEOS_EXPORT bool GetAutoRepeatRateForTesting(
      AutoRepeatRate* out_rate);

  // Returns false if |layout_name| contains a bad character.
  static CHROMEOS_EXPORT bool CheckLayoutNameForTesting(
      const std::string& layout_name);

  // Note: At this moment, classes other than InputMethodManager should not
  // instantiate the ImeKeyboard class.
  static ImeKeyboard* Create();
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROMEOS_IME_IME_KEYBOARD_H_
