// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_HOTKEY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_HOTKEY_MANAGER_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/observer_list.h"

typedef union _XEvent XEvent;

namespace chromeos {
namespace input_method {

// A class which holds all hotkeys for input method switching.
class HotkeyManager {
 public:
  class Observer {
   public:
    virtual ~Observer() {}
    // Called when one of the input method hotkeys is pressed.
    virtual void HotkeyPressed(HotkeyManager* manager, int event_id) = 0;
  };

  HotkeyManager();
  ~HotkeyManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Registers a hotkey to the manager. For example, to handle Control+space,
  // you can call AddHotkey(kNextInputMethod, XK_space, ControlMask, true).
  // Returns false if |event_id| is negative, or the same hotkey is already
  // registered.
  bool AddHotkey(int event_id,
                 uint32 keysym,  // one of XK_* in X11/keysymdef.h.
                 uint32 modifiers,  // set of *Mask flags in X11/X.h, or zero.
                 bool trigger_on_key_press);
  // TODO(yusukes): Add RemoveHotkey function if we support crosbug.com/6267.

  // Checks if |key_event| is a hotkey. If it is, Observer::HotkeyPressed() is
  // called for each observers. This function returns true if the key_event
  // should be consumed.
  bool FilterKeyEvent(const XEvent& key_event);

 protected:
  // Note: These functions and variables are protected for testability.

  // Returns true if |keysym| is a modifier (e.g. XK_Shift_L).
  bool IsModifier(uint32 keysym) const;

  // Converts |keysym| and |is_key_press| into modifiers, and add them to
  // |modifiers|. For example, if |keysym| is XK_Shift_L, |modifiers| is
  // ControlMask, and |is_key_press| is false, the function would return
  // (ShiftMask | ControlMask | kKeyReleaseMask).
  uint32 NormalizeModifiers(
      uint32 keysym, uint32 modifiers, bool is_key_press) const;

  // Called by the public function, FilterKeyEvent, and returns an event_id or
  // kNoEvent. Does not communicate with |observers_|.
  int FilterKeyEventInternal(
      uint32 keysym, uint32 modifiersm, bool is_key_press);

  static const uint32 kKeyReleaseMask;
  static const int kNoEvent;
  static const int kFiltered;

 private:
  // A map from hotkey (i.e. a pair of keysym and normalized modifier) to
  // event_id.
  std::map<std::pair<uint32, uint32>, int> hotkeys_;

  uint32 previous_keysym_;
  uint32 previous_modifiers_;
  bool filter_release_events_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(HotkeyManager);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_HOTKEY_MANAGER_H_
