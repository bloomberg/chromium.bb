// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_EVENT_REWRITER_H_
#define CHROME_BROWSER_UI_ASH_EVENT_REWRITER_H_

#include <map>
#include <string>

#include "ash/event_rewriter_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "ui/aura/root_window_observer.h"
#include "ui/base/keycodes/keyboard_codes.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/device_hierarchy_observer.h"
#endif

class PrefService;

namespace aura {
class RootWindow;
}

#if defined(OS_CHROMEOS)
namespace chromeos {
namespace input_method {
class XKeyboard;
}
}
#endif

class EventRewriter : public ash::EventRewriterDelegate,
                      public aura::RootWindowObserver
#if defined(OS_CHROMEOS)
                    , public chromeos::DeviceHierarchyObserver
#endif
{
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
  void RewriteForTesting(ui::KeyEvent* event);

  const std::map<int, DeviceType>& device_id_to_type_for_testing() const {
    return device_id_to_type_;
  }
  void set_last_device_id_for_testing(int device_id) {
    last_device_id_ = device_id;
  }
  void set_pref_service_for_testing(const PrefService* pref_service) {
    pref_service_ = pref_service;
  }
#if defined(OS_CHROMEOS)
  void set_xkeyboard_for_testing(chromeos::input_method::XKeyboard* xkeyboard) {
    xkeyboard_ = xkeyboard;
  }
#endif

  // Gets DeviceType from the |device_name|.
  static DeviceType GetDeviceType(const std::string& device_name);

 private:
  // ash::EventRewriterDelegate overrides:
  virtual ash::EventRewriterDelegate::Action RewriteOrFilterKeyEvent(
      ui::KeyEvent* event) OVERRIDE;
  virtual ash::EventRewriterDelegate::Action RewriteOrFilterLocatedEvent(
      ui::LocatedEvent* event) OVERRIDE;

  // aura::RootWindowObserver overrides:
  virtual void OnKeyboardMappingChanged(const aura::RootWindow* root) OVERRIDE;

#if defined(OS_CHROMEOS)
  // chromeos::DeviceHierarchyObserver overrides:
  virtual void DeviceHierarchyChanged() OVERRIDE {}
  virtual void DeviceAdded(int device_id) OVERRIDE;
  virtual void DeviceRemoved(int device_id) OVERRIDE;
  virtual void DeviceKeyPressedOrReleased(int device_id) OVERRIDE;

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
    unsigned int input_mods;
    unsigned int input_native_mods;
    KeySym output_keysym;
    ui::KeyboardCode output_keycode;
    unsigned int output_mods;
    unsigned int output_native_mods;
  };

  // Given a set of KeyboardRemapping structs, it finds a matching struct
  // if possible, and updates the remapped event values. Returns true if a
  // remapping was found and remapped values were updated.
  bool RewriteWithKeyboardRemappingsByKeySym(
      const KeyboardRemapping* remappings,
      size_t num_remappings,
      KeySym keysym,
      unsigned int native_mods,
      unsigned int mods,
      KeySym* remapped_native_keysym,
      unsigned int* remapped_native_mods,
      ui::KeyboardCode* remapped_keycode,
      unsigned int* remapped_mods);

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
      unsigned int mods,
      KeySym* remapped_native_keysym,
      unsigned int* remapped_native_mods,
      ui::KeyboardCode* remapped_keycode,
      unsigned int* remapped_mods);
#endif

  // Rewrites the |event| by applying all RewriteXXX functions as needed.
  void Rewrite(ui::KeyEvent* event);

  // Rewrites a modifier key press/release following the current user
  // preferences.
  bool RewriteModifiers(ui::KeyEvent* event);

  // Rewrites Fn key press/release to Control. In some cases, Fn key is not
  // intercepted by the EC, but generates a key event like "XK_F15 + Mod3Mask"
  // as shown in crosbug.com/p/14339.
  bool RewriteFnKey(ui::KeyEvent* event);

  // Rewrites a NumPad key press/release without Num Lock to a corresponding key
  // press/release with the lock.  Returns true when |event| is rewritten.
  bool RewriteNumPadKeys(ui::KeyEvent* event);

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
  bool RewriteExtendedKeys(ui::KeyEvent* event);

  // When the Search key acts as a function key, it remaps Search+1
  // through Search+= to F1 through F12. Returns true when the |event| is
  // rewritten.
  bool RewriteFunctionKeys(ui::KeyEvent* event);

  // Rewrites the located |event|.
  void RewriteLocatedEvent(ui::LocatedEvent* event);

  // Overwrites |event| with the keycodes and flags.
  void OverwriteEvent(ui::KeyEvent* event,
                      unsigned int new_native_keycode,
                      unsigned int new_native_state,
                      ui::KeyboardCode new_keycode,
                      int new_flags);

  // Checks the type of the |device_name|, and inserts a new entry to
  // |device_id_to_type_|.
  DeviceType DeviceAddedInternal(int device_id, const std::string& device_name);

  // Returns true if |last_device_id_| is Apple's.
  bool IsAppleKeyboard() const;

  // Remaps |original_flags| to |remapped_flags| and |original_native_modifiers|
  // to |remapped_native_modifiers| following the current user prefs.
  void GetRemappedModifierMasks(int original_flags,
                                unsigned int original_native_modifiers,
                                int* remapped_flags,
                                unsigned int* remapped_native_modifiers) const;

  std::map<int, DeviceType> device_id_to_type_;
  int last_device_id_;

#if defined(OS_CHROMEOS)
  // A mapping from X11 KeySym keys to KeyCode values.
  base::hash_map<unsigned long, unsigned long> keysym_to_keycode_map_;

  chromeos::input_method::XKeyboard* xkeyboard_;  // for testing.
#endif

  const PrefService* pref_service_;  // for testing.

  DISALLOW_COPY_AND_ASSIGN(EventRewriter);
};

#endif  // CHROME_BROWSER_UI_ASH_EVENT_REWRITER_H_
