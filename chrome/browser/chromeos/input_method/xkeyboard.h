// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_XKEYBOARD_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_XKEYBOARD_H_
#pragma once

#include <queue>
#include <set>
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

class InputMethodUtil;

class XKeyboard {
 public:
  // Note: at this moment, classes other than InputMethodManager should not
  // instantiate the XKeyboard class.
  explicit XKeyboard(const InputMethodUtil& util);
  ~XKeyboard();

  // Sets the current keyboard layout to |layout_name|. This function does not
  // change the current mapping of the modifier keys. Returns true on success.
  bool SetCurrentKeyboardLayoutByName(const std::string& layout_name);

  // Remaps modifier keys. This function does not change the current keyboard
  // layout. Returns true on success. For now, you can't remap Left Control and
  // Left Alt keys to caps lock.
  bool RemapModifierKeys(const ModifierMap& modifier_map);

  // Sets the current keyboard layout again. We have to call the function every
  // time when "XI_HierarchyChanged" XInput2 event is sent to Chrome. See
  // xinput_hierarchy_changed_event_listener.h for details.
  bool ReapplyCurrentKeyboardLayout();

  // Updates keyboard LEDs on all keyboards.
  // XKB asymmetrically propagates keyboard modifier indicator state changes to
  // slave keyboards. If the state change is initiated from a client to the
  // "core/master keyboard", XKB changes global state and pushes an indication
  // change down to all keyboards. If the state change is initiated by one slave
  // (physical) keyboard, it changes global state but only pushes an indicator
  // state change down to that one keyboard.
  // This function changes LEDs on all keyboards by explicitly updating the
  // core/master keyboard.
  void ReapplyCurrentModifierLockStatus();

  // Sets the Caps Lock and Num Lock status. Do not call the function from
  // non-UI threads.
  void SetLockedModifiers(ModifierLockStatus new_caps_lock_status,
                          ModifierLockStatus new_num_lock_status);

  // Sets the num lock status to |enable_num_lock|. Do not call the function
  // from non-UI threads.
  void SetNumLockEnabled(bool enable_num_lock);

  // Sets the caps lock status to |enable_caps_lock|. Do not call the function
  // from non-UI threads.
  void SetCapsLockEnabled(bool enable_caps_lock);

  // Set true on |out_caps_lock_enabled| if Caps Lock is enabled. Set true on
  // |out_num_lock_enabled| if Num Lock (or to be precise, the modifier
  // specified by |num_lock_mask|) is enabled. Both 'out' parameters can be
  // NULL. When |out_num_lock_enabled| is NULL, |num_lock_mask| is ignored (you
  // can pass 0 in this case). Do not call the function from non-UI threads.
  static void GetLockedModifiers(unsigned int num_lock_mask,
                                 bool* out_caps_lock_enabled,
                                 bool* out_num_lock_enabled);

  // Returns true if num lock is enabled. Do not call the function from non-UI
  // threads.
  static bool NumLockIsEnabled(unsigned int num_lock_mask);

  // Returns true if caps lock is enabled. Do not call the function from non-UI
  // threads.
  static bool CapsLockIsEnabled();

  // Turns on and off the auto-repeat of the keyboard. Returns true on success.
  // Do not call the function from non-UI threads.
  static bool SetAutoRepeatEnabled(bool enabled);

  // Sets the auto-repeat rate of the keyboard, initial delay in ms, and repeat
  // interval in ms.  Returns true on success. Do not call the function from
  // non-UI threads.
  static bool SetAutoRepeatRate(const AutoRepeatRate& rate);

  // Returns a mask (e.g. 1U<<4) for Num Lock. On error, returns 0.
  static unsigned int GetNumLockMask();

 protected:
  // Creates a full XKB layout name like
  //   "gb(extd)+chromeos(leftcontrol_disabled_leftalt),us"
  // from modifier key mapping and |layout_name|, such as "us", "us(dvorak)",
  // and "gb(extd)". Returns an empty string on error. This function is
  // protected: for testability.
  std::string CreateFullXkbLayoutName(const std::string& layout_name,
                                      const ModifierMap& modifire_map);

  // Returns true if |key| is in |modifier_map| as replacement. This function is
  // protected: for testability.
  static bool ContainsModifierKeyAsReplacement(const ModifierMap& modifier_map,
                                               ModifierKey key);

 private:
  // This function is used by SetLayout() and RemapModifierKeys(). Calls
  // setxkbmap command if needed, and updates the last_full_layout_name_ cache.
  bool SetLayoutInternal(const std::string& layout_name,
                         const ModifierMap& modifier_map,
                         bool force);

  // Executes 'setxkbmap -layout ...' command asynchronously using a layout name
  // in the |execute_queue_|. Do nothing if the queue is empty.
  // TODO(yusukes): Use libxkbfile.so instead of the command (crosbug.com/13105)
  void MaybeExecuteSetLayoutCommand();

  // Returns true if the XKB layout uses the right Alt key for special purposes
  // like AltGr.
  bool KeepRightAlt(const std::string& xkb_layout_name) const;

  // Returns true if the XKB layout uses the CapsLock key for special purposes.
  // For example, since US Colemak layout uses the key as back space,
  // KeepCapsLock("us(colemak)") would return true.
  bool KeepCapsLock(const std::string& xkb_layout_name) const;

  // Converts |key| to a modifier key name which is used in
  // /usr/share/X11/xkb/symbols/chromeos.
  static std::string ModifierKeyToString(ModifierKey key);

  // Called when execve'd setxkbmap process exits.
  static void OnSetLayoutFinish(pid_t pid, int status, XKeyboard* self);

  const bool is_running_on_chrome_os_;
  unsigned int num_lock_mask_;

  // The current Num Lock and Caps Lock status. If true, enabled.
  bool current_num_lock_status_;
  bool current_caps_lock_status_;
  // The XKB layout name which we set last time like "us" and "us(dvorak)".
  std::string current_layout_name_;
  // The mapping of modifier keys we set last time.
  ModifierMap current_modifier_map_;

  // A queue for executing setxkbmap one by one.
  std::queue<std::string> execute_queue_;

  std::set<std::string> keep_right_alt_xkb_layout_names_;
  std::set<std::string> caps_lock_remapped_xkb_layout_names_;

  DISALLOW_COPY_AND_ASSIGN(XKeyboard);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_XKEYBOARD_H_
