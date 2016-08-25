// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EVENTS_EVENT_REWRITER_H_
#define CHROME_BROWSER_CHROMEOS_EVENTS_EVENT_REWRITER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/dom/dom_key.h"

class PrefService;

namespace ash {
class StickyKeysController;
}

namespace ui {
enum class DomCode;
};

namespace chromeos {
namespace input_method {
class ImeKeyboard;
}

// EventRewriter makes various changes to keyboard-related events,
// including KeyEvents and some other events with keyboard modifier flags:
// - maps certain non-character keys according to user preferences
//   (Control, Alt, Search, Caps Lock, Escape, Backspace, Diamond);
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
    kDeviceHotrodRemote,
    kDeviceVirtualCoreKeyboard,  // X-server generated events.
  };

  // Things that keyboard-related rewriter phases can change about an Event.
  struct MutableKeyState {
    int flags;
    ui::DomCode code;
    ui::DomKey::Base key;
    ui::KeyboardCode key_code;
  };

  // Does not take ownership of the |sticky_keys_controller|, which may also
  // be NULL (for testing without ash), in which case sticky key operations
  // don't happen.
  explicit EventRewriter(ash::StickyKeysController* sticky_keys_controller);
  ~EventRewriter() override;

  // Calls KeyboardDeviceAddedInternal.
  DeviceType KeyboardDeviceAddedForTesting(int device_id,
                                           const std::string& device_name);

  // Calls RewriteMouseEvent().
  void RewriteMouseButtonEventForTesting(
      const ui::MouseEvent& event,
      std::unique_ptr<ui::Event>* rewritten_event);

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
  ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* rewritten_event) override;
  ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      std::unique_ptr<ui::Event>* new_event) override;

  // Generate a new key event from an original key event and the replacement
  // state determined by a key rewriter.
  static void BuildRewrittenKeyEvent(
      const ui::KeyEvent& key_event,
      const MutableKeyState& state,
      std::unique_ptr<ui::Event>* rewritten_event);

 private:
  void DeviceKeyPressedOrReleased(int device_id);

  // Returns the PrefService that should be used.
  const PrefService* GetPrefService() const;

  // Adds a device to |device_id_to_type_|.
  DeviceType KeyboardDeviceAdded(int device_id);

  // Checks the type of the |device_name|, |vendor_id| and |product_id|, and
  // inserts a new entry to |device_id_to_type_|.
  DeviceType KeyboardDeviceAddedInternal(int device_id,
                                         const std::string& device_name,
                                         int vendor_id,
                                         int product_id);

  // Returns true if |last_keyboard_device_id_| is Apple's.
  bool IsAppleKeyboard() const;
  // Returns true if |last_keyboard_device_id_| is Hotrod remote.
  bool IsHotrodRemote() const;
  // Returns true if |last_keyboard_device_id_| is of given |device_type|.
  bool IsLastKeyboardOfType(DeviceType device_type) const;

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

  // Rewrite a particular kind of event.
  ui::EventRewriteStatus RewriteKeyEvent(
      const ui::KeyEvent& key_event,
      std::unique_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus RewriteMouseButtonEvent(
      const ui::MouseEvent& mouse_event,
      std::unique_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus RewriteMouseWheelEvent(
      const ui::MouseWheelEvent& mouse_event,
      std::unique_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus RewriteTouchEvent(
      const ui::TouchEvent& touch_event,
      std::unique_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus RewriteScrollEvent(
      const ui::ScrollEvent& scroll_event,
      std::unique_ptr<ui::Event>* rewritten_event);

  // Rewriter phases. These can inspect the original |event|, but operate using
  // the current |state|, which may have been modified by previous phases.
  bool RewriteModifierKeys(const ui::KeyEvent& event, MutableKeyState* state);
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

  // Some keyboard layouts have 'latching' keys, which either apply
  // a modifier while held down (like normal modifiers), or, if no
  // non-modifier is pressed while the latching key is down, apply the
  // modifier to the next non-modifier keypress. Under Ozone the stateless
  // layout model requires this to be handled explicitly. See crbug.com/518237
  // Pragmatically this, like the Diamond key, is handled here in
  // EventRewriter, but modifier state management is scattered between
  // here, sticky keys, and the system layer (X11 or Ozone), and could
  // do with refactoring.
  // - |pressed_modifier_latches_| records the latching keys currently pressed.
  //   It also records the active modifier flags for non-modifier keys that are
  //   remapped to modifiers, e.g. Diamond/F15.
  // - |latched_modifier_latches_| records the latching keys just released,
  //   to be applied to the next non-modifier key.
  // - |used_modifier_latches_| records the latching keys applied to a non-
  //   modifier while pressed, so that they do not get applied after release.
  int pressed_modifier_latches_;
  int latched_modifier_latches_;
  int used_modifier_latches_;

  DISALLOW_COPY_AND_ASSIGN(EventRewriter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EVENTS_EVENT_REWRITER_H_
