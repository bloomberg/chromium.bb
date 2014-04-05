// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EVENTS_EVENT_REWRITER_H_
#define CHROME_BROWSER_CHROMEOS_EVENTS_EVENT_REWRITER_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_pump_observer.h"
#include "chrome/browser/chromeos/device_hierarchy_observer.h"
#include "ui/events/keycodes/keyboard_codes.h"

class PrefService;
typedef union _XEvent XEvent;

namespace chromeos {
class KeyboardDrivenEventRewriter;
namespace input_method {
class XKeyboard;
}

class EventRewriter : public DeviceHierarchyObserver,
                      public base::MessagePumpObserver {
 public:
  enum DeviceType {
    kDeviceUnknown = 0,
    kDeviceAppleKeyboard,
  };

  EventRewriter();
  virtual ~EventRewriter();

  // Calls DeviceAddedInternal.
  DeviceType DeviceAddedForTesting(int device_id,
                                   const std::string& device_name);
  // Calls Rewrite.
  void RewriteForTesting(XEvent* event);

  const std::map<int, DeviceType>& device_id_to_type_for_testing() const {
    return device_id_to_type_;
  }
  void set_last_device_id_for_testing(int device_id) {
    last_device_id_ = device_id;
  }
  void set_pref_service_for_testing(const PrefService* pref_service) {
    pref_service_for_testing_ = pref_service;
  }
  void set_xkeyboard_for_testing(input_method::XKeyboard* xkeyboard) {
    xkeyboard_for_testing_ = xkeyboard;
  }

  // Gets DeviceType from the |device_name|.
  static DeviceType GetDeviceType(const std::string& device_name);

 private:
  friend class EventRewriterAshTest;
  friend class EventRewriterTest;

  void DeviceKeyPressedOrReleased(int device_id);

  // base::MessagePumpObserver overrides:
  virtual void WillProcessEvent(const base::NativeEvent& event) OVERRIDE;
  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE;

  // DeviceHierarchyObserver overrides:
  virtual void DeviceHierarchyChanged() OVERRIDE;
  virtual void DeviceAdded(int device_id) OVERRIDE;
  virtual void DeviceRemoved(int device_id) OVERRIDE;

  // We don't want to include Xlib.h here since it has polluting macros, so
  // define these locally.
  typedef unsigned long KeySym;
  typedef unsigned char KeyCode;

  // Updates |*_xkeycode_| in response to a keyboard map change.
  void RefreshKeycodes();
  // Converts an X key symbol like XK_Control_L to a key code.
  unsigned char NativeKeySymToNativeKeycode(KeySym keysym);

  struct KeyboardRemapping {
    KeySym input_keysym;
    unsigned int input_native_mods;
    KeySym output_keysym;
    unsigned int output_native_mods;
  };

  // Returns true if the target for |event| would prefer to receive raw function
  // keys instead of having them rewritten into back, forward, brightness,
  // volume, etc. or if the user has specified that they desire top-row keys to
  // be treated as function keys globally.
  bool TopRowKeysAreFunctionKeys(XEvent* event) const;

  // Given a set of KeyboardRemapping structs, it finds a matching struct
  // if possible, and updates the remapped event values. Returns true if a
  // remapping was found and remapped values were updated.
  bool RewriteWithKeyboardRemappingsByKeySym(
      const KeyboardRemapping* remappings,
      size_t num_remappings,
      KeySym keysym,
      unsigned int native_mods,
      KeySym* remapped_native_keysym,
      unsigned int* remapped_native_mods);

  // Given a set of KeyboardRemapping structs, it finds a matching struct
  // if possible, and updates the remapped event values. This function converts
  // the KeySym in the KeyboardRemapping struct into the KeyCode before matching
  // to allow any KeyCode on the same physical key as the given KeySym to match.
  // Returns true if a remapping was found and remapped values were updated.
  bool RewriteWithKeyboardRemappingsByKeyCode(
      const KeyboardRemapping* remappings,
      size_t num_remappings,
      KeyCode keycode,
      unsigned int native_mods,
      KeySym* remapped_native_keysym,
      unsigned int* remapped_native_mods);

  // Returns the PrefService that should be used.
  const PrefService* GetPrefService() const;

  // Rewrites the |event| by applying all RewriteXXX functions as needed.
  void Rewrite(XEvent* event);

  // Rewrites a modifier key press/release following the current user
  // preferences.
  bool RewriteModifiers(XEvent* event);

  // Rewrites Fn key press/release to Control. In some cases, Fn key is not
  // intercepted by the EC, but generates a key event like "XK_F15 + Mod3Mask"
  // as shown in crosbug.com/p/14339.
  bool RewriteFnKey(XEvent* event);

  // Rewrites a NumPad key press/release without Num Lock to a corresponding key
  // press/release with the lock.  Returns true when |event| is rewritten.
  bool RewriteNumPadKeys(XEvent* event);

  // Rewrites Backspace and Arrow keys following the Chrome OS keyboard spec.
  //  * Alt+Backspace -> Delete
  //  * Alt+Up -> Prior (aka PageUp)
  //  * Alt+Down -> Next (aka PageDown)
  //  * Ctrl+Alt+Up -> Home
  //  * Ctrl+Alt+Down -> End
  // When the Search key acts as a function key, it instead maps:
  //  * Search+Backspace -> Delete
  //  * Search+Up -> Prior (aka PageUp)
  //  * Search+Down -> Next (aka PageDown)
  //  * Search+Left -> Home
  //  * Search+Right -> End
  //  * Search+. -> Insert
  // Returns true when the |event| is rewritten.
  bool RewriteExtendedKeys(XEvent* event);

  // When the Search key acts as a function key, it remaps Search+1
  // through Search+= to F1 through F12. Returns true when the |event| is
  // rewritten.
  bool RewriteFunctionKeys(XEvent* event);

  // Rewrites the located |event|.
  void RewriteLocatedEvent(XEvent* event);

  // Overwrites |event| with the keycodes and flags.
  void OverwriteEvent(XEvent* event,
                      unsigned int new_native_keycode,
                      unsigned int new_native_state);

  // Checks the type of the |device_name|, and inserts a new entry to
  // |device_id_to_type_|.
  DeviceType DeviceAddedInternal(int device_id, const std::string& device_name);

  // Returns true if |last_device_id_| is Apple's.
  bool IsAppleKeyboard() const;

  // Remaps |original_native_modifiers| to |remapped_native_modifiers| following
  // the current user prefs.
  void GetRemappedModifierMasks(unsigned int original_native_modifiers,
                                unsigned int* remapped_native_modifiers) const;

  std::map<int, DeviceType> device_id_to_type_;
  int last_device_id_;

  // A mapping from X11 KeySym keys to KeyCode values.
  base::hash_map<unsigned long, unsigned long> keysym_to_keycode_map_;

  // A set of device IDs whose press event has been rewritten.
  std::set<int> pressed_device_ids_;

  input_method::XKeyboard* xkeyboard_for_testing_;

  scoped_ptr<KeyboardDrivenEventRewriter>
      keyboard_driven_event_rewriter_;

  const PrefService* pref_service_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(EventRewriter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EVENTS_EVENT_REWRITER_H_
