// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EVENTS_EVENT_REWRITER_H_
#define CHROME_BROWSER_CHROMEOS_EVENTS_EVENT_REWRITER_H_

#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"

class PrefService;

namespace ash {
class StickyKeysController;
}

namespace chromeos {
namespace input_method {
class ImeKeyboard;
}

// EventRewriter makes various changes to keyboard-related events,
// including KeyEvents and some other events with keyboard modifier flags:
// - maps modifiers keys (Control, Alt, Search, Caps, Diamond) according
//   to user preferences;
// - maps Command to Control on Apple keyboards;
// - converts numeric pad editing keys to their numeric forms;
// - converts top-row function keys to special keys where necessary;
// - handles various key combinations like Search+Backspace -> Delete
//   and Search+number to Fnumber;
// - handles key/pointer combinations like Alt+Button1 -> Button3.
class EventRewriter : public ui::EventRewriter {
 public:
  enum DeviceType {
    kDeviceUnknown = 0,
    kDeviceAppleKeyboard,
  };

  // Does not take ownership of the |sticky_keys_controller|, which may also
  // be NULL (for testing without ash), in which case sticky key operations
  // don't happen.
  explicit EventRewriter(ash::StickyKeysController* sticky_keys_controller);
  virtual ~EventRewriter();

  // Calls KeyboardDeviceAddedInternal.
  DeviceType KeyboardDeviceAddedForTesting(int device_id,
                                           const std::string& device_name);

  // Calls RewriteMouseEvent().
  void RewriteMouseButtonEventForTesting(
      const ui::MouseEvent& event,
      scoped_ptr<ui::Event>* rewritten_event);

  const std::map<int, DeviceType>& device_id_to_type_for_testing() const {
    return device_id_to_type_;
  }
  void set_last_keyboard_device_id_for_testing(int device_id) {
    last_keyboard_device_id_ = device_id;
  }
  void set_pref_service_for_testing(const PrefService* pref_service) {
    pref_service_for_testing_ = pref_service;
  }
  void set_ime_keyboard_for_testing(
      chromeos::input_method::ImeKeyboard* ime_keyboard) {
    ime_keyboard_for_testing_ = ime_keyboard;
  }

  // EventRewriter overrides:
  virtual ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      scoped_ptr<ui::Event>* rewritten_event) OVERRIDE;
  virtual ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      scoped_ptr<ui::Event>* new_event) OVERRIDE;

  // Generate a new key event from an original key event and the replacement
  // key code and flags determined by a key rewriter.
  static void BuildRewrittenKeyEvent(const ui::KeyEvent& key_event,
                                     ui::KeyboardCode key_code,
                                     int flags,
                                     scoped_ptr<ui::Event>* rewritten_event);

 private:
  // Things that keyboard-related rewriter phases can change about an Event.
  struct MutableKeyState {
    int flags;
    ui::KeyboardCode key_code;
  };

  // Tables of direct remappings for |RewriteWithKeyboardRemappingsByKeyCode()|.
  struct KeyboardRemapping {
    ui::KeyboardCode input_key_code;
    int input_flags;
    ui::KeyboardCode output_key_code;
    int output_flags;
  };

  void DeviceKeyPressedOrReleased(int device_id);

  // Returns the PrefService that should be used.
  const PrefService* GetPrefService() const;

  // Adds a device to |device_id_to_type_|.
  void KeyboardDeviceAdded(int device_id);

  // Checks the type of the |device_name|, and inserts a new entry to
  // |device_id_to_type_|.
  DeviceType KeyboardDeviceAddedInternal(int device_id,
                                         const std::string& device_name);

  // Returns true if |last_keyboard_device_id_| is Apple's.
  bool IsAppleKeyboard() const;

  // Returns true if the target for |event| would prefer to receive raw function
  // keys instead of having them rewritten into back, forward, brightness,
  // volume, etc. or if the user has specified that they desire top-row keys to
  // be treated as function keys globally.
  bool TopRowKeysAreFunctionKeys(const ui::KeyEvent& event) const;

  // Given modifier flags |original_flags|, returns the remapped modifiers
  // according to user preferences and/or event properties.
  int GetRemappedModifierMasks(const PrefService& pref_service,
                               const ui::Event& event,
                               int original_flags) const;

  // Given a set of KeyboardRemapping structs, it finds a matching struct
  // if possible, and updates the remapped event values. Returns true if a
  // remapping was found and remapped values were updated.
  bool RewriteWithKeyboardRemappingsByKeyCode(
      const KeyboardRemapping* remappings,
      size_t num_remappings,
      const MutableKeyState& input,
      MutableKeyState* remapped_state);

  // Rewrite a particular kind of event.
  ui::EventRewriteStatus RewriteKeyEvent(
      const ui::KeyEvent& key_event,
      scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus RewriteMouseButtonEvent(
      const ui::MouseEvent& mouse_event,
      scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus RewriteMouseWheelEvent(
      const ui::MouseWheelEvent& mouse_event,
      scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus RewriteTouchEvent(
      const ui::TouchEvent& touch_event,
      scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus RewriteScrollEvent(
      const ui::ScrollEvent& scroll_event,
      scoped_ptr<ui::Event>* rewritten_event);

  // Rewriter phases. These can inspect the original |event|, but operate using
  // the current |state|, which may have been modified by previous phases.
  void RewriteModifierKeys(const ui::KeyEvent& event, MutableKeyState* state);
  void RewriteNumPadKeys(const ui::KeyEvent& event, MutableKeyState* state);
  void RewriteExtendedKeys(const ui::KeyEvent& event, MutableKeyState* state);
  void RewriteFunctionKeys(const ui::KeyEvent& event, MutableKeyState* state);
  void RewriteLocatedEvent(const ui::Event& event, int* flags);
  int RewriteModifierClick(const ui::MouseEvent& event, int* flags);

  // A set of device IDs whose press event has been rewritten.
  // This is to ensure that press and release events are rewritten consistently.
  std::set<int> pressed_device_ids_;

  std::map<int, DeviceType> device_id_to_type_;

  // The |source_device_id()| of the most recent keyboard event,
  // used to interpret modifiers on pointer events.
  int last_keyboard_device_id_;

  chromeos::input_method::ImeKeyboard* ime_keyboard_for_testing_;
  const PrefService* pref_service_for_testing_;

  // The sticky keys controller is not owned here;
  // at time of writing it is a singleton in ash::Shell.
  ash::StickyKeysController* sticky_keys_controller_;

  // The ChromeOS Diamond key arrives as F15. Since F15 is not a modifier,
  // we need to track its pressed state explicitly, and apply the selected
  // modifier flag to key and mouse presses that arrive while F15 is down.
  // While the Diamond key is down, this holds the corresponding modifier
  // ui::EventFlags; otherwise it is EF_NONE.
  int current_diamond_key_modifier_flags_;

  DISALLOW_COPY_AND_ASSIGN(EventRewriter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EVENTS_EVENT_REWRITER_H_
