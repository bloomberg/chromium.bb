// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STICKY_KEYS_STICKY_KEYS_CONTROLLER_H_
#define ASH_STICKY_KEYS_STICKY_KEYS_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/sticky_keys/sticky_keys_state.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"

namespace ui {
class Event;
class KeyEvent;
class MouseEvent;
}  // namespace ui

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class StickyKeysOverlay;
class StickyKeysHandler;

// StickyKeysController is an accessibility feature for users to be able to
// compose key and mouse event with modifier keys without simultaneous key
// press event. Instead they can compose events separately pressing each of the
// modifier keys involved.
// e.g. Composing Ctrl + T
//       User Action   : The KeyEvent widget will receives
// ----------------------------------------------------------
// 1. Press Ctrl key   : Ctrl Keydown.
// 2. Release Ctrl key : No event
// 3. Press T key      : T keydown event with ctrl modifier.
// 4.                  : Ctrl Keyup
// 5. Release T key    : T keyup without ctrl modifier (Windows behavior)
//
// By typing same modifier keys twice, users can generate bunch of modified key
// events.
// e.g. To focus tabs consistently by Ctrl + 1, Ctrl + 2 ...
//       User Action   : The KeyEvent widget will receives
// ----------------------------------------------------------
// 1. Press Ctrl key   : Ctrl Keydown
// 2. Release Ctrl key : No event
// 3. Press Ctrl key   : No event
// 4. Release Ctrl key : No event
// 5. Press 1 key      : 1 Keydown event with Ctrl modifier.
// 6. Release 1 key    : 1 Keyup event with Ctrl modifier.
// 7. Press 2 key      : 2 Keydown event with Ctrl modifier.
// 8. Release 2 key    : 2 Keyup event with Ctrl modifier.
// 9. Press Ctrl key   : No event
// 10. Release Ctrl key: Ctrl Keyup
//
// In the case of Chrome OS, StickyKeysController supports Shift,Alt,Ctrl
// modifiers. Each handling or state is performed independently.
//
// StickyKeysController is disabled by default.
class ASH_EXPORT StickyKeysController : public ui::EventHandler {
 public:
  StickyKeysController();
  virtual ~StickyKeysController();

  // Activate sticky keys to intercept and modify incoming events.
  void Enable(bool enabled);

  void SetModifiersEnabled(bool mod3_enabled, bool altgr_enabled);

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;

  // Returns the StickyKeyOverlay used by the controller. Ownership is not
  // passed.
  StickyKeysOverlay* GetOverlayForTest();

 private:
  // Handles keyboard event. Returns true if Sticky key consumes keyboard event.
  bool HandleKeyEvent(ui::KeyEvent* event);

  // Handles mouse event. Returns true if sticky key consumes mouse event.
  bool HandleMouseEvent(ui::MouseEvent* event);

  // Handles scroll event. Returns true if sticky key consumes scroll event.
  bool HandleScrollEvent(ui::ScrollEvent* event);

  // Updates the overlay UI with the current state of the sticky keys.
  void UpdateOverlay();

  // Whether sticky keys is activated and modifying events.
  bool enabled_;

  // Whether the current layout has a mod3 key.
  bool mod3_enabled_;

  // Whether the current layout has an altgr key.
  bool altgr_enabled_;

  // Sticky key handlers.
  scoped_ptr<StickyKeysHandler> shift_sticky_key_;
  scoped_ptr<StickyKeysHandler> alt_sticky_key_;
  scoped_ptr<StickyKeysHandler> altgr_sticky_key_;
  scoped_ptr<StickyKeysHandler> ctrl_sticky_key_;

  scoped_ptr<StickyKeysOverlay> overlay_;

  DISALLOW_COPY_AND_ASSIGN(StickyKeysController);
};

// StickyKeysHandler handles key event and controls sticky keysfor specific
// modifier keys. If monitored keyboard events are recieved, StickyKeysHandler
// changes internal state. If non modifier keyboard events or mouse events are
// received, StickyKeysHandler will append modifier based on internal state.
// For other events, StickyKeysHandler does nothing.
//
// The DISABLED state is default state and any incoming non modifier keyboard
// events will not be modified. The ENABLED state is one shot modification
// state. Only next keyboard event will be modified. After that, internal state
// will be back to DISABLED state with sending modifier keyup event. In the case
// of LOCKED state, all incomming keyboard events will be modified. The LOCKED
// state will be back to DISABLED state by next monitoring modifier key.
//
// The detailed state flow as follows:
//                                     Current state
//                  |   DISABLED    |    ENABLED     |    LOCKED   |
// ----------------------------------------------------------------|
// Modifier KeyDown |   noop        |    noop(*)     |    noop(*)  |
// Modifier KeyUp   | To ENABLED(*) | To LOCKED(*)   | To DISABLED |
// Normal KeyDown   |   noop        | To DISABLED(#) |    noop(#)  |
// Normal KeyUp     |   noop        |    noop        |    noop(#)  |
// Other KeyUp/Down |   noop        |    noop        |    noop     |
// Mouse Press      |   noop        |    noop(#)     |    noop(#)  |
// Mouse Release    |   noop        | To DISABLED(#) |    noop(#)  |
// Mouse Wheel      |   noop        | To DISABLED(#) |    noop(#)  |
// Other Mouse Event|   noop        |    noop        |    noop     |
//
// Here, (*) means key event will be consumed by StickyKeys, and (#) means event
// is modified.
class ASH_EXPORT StickyKeysHandler {
 public:
  class StickyKeysHandlerDelegate {
   public:
    StickyKeysHandlerDelegate();
    virtual ~StickyKeysHandlerDelegate();

    // Dispatches keyboard event synchronously.
    virtual void DispatchKeyEvent(ui::KeyEvent* event,
                                  aura::Window* target) = 0;

    // Dispatches mouse event synchronously.
    virtual void DispatchMouseEvent(ui::MouseEvent* event,
                                    aura::Window* target) = 0;

    // Dispatches scroll event synchronously.
    virtual void DispatchScrollEvent(ui::ScrollEvent* event,
                                     aura::Window* target) = 0;
  };

  // This class takes an ownership of |delegate|.
  StickyKeysHandler(ui::EventFlags modifier_flag,
                    StickyKeysHandlerDelegate* delegate);
  ~StickyKeysHandler();

  // Handles key event. Returns true if key is consumed.
  bool HandleKeyEvent(ui::KeyEvent* event);

  // Handles a mouse event. Returns true if mouse event is consumed.
  bool HandleMouseEvent(ui::MouseEvent* event);

  // Handles a scroll event. Returns true if scroll event is consumed.
  bool HandleScrollEvent(ui::ScrollEvent* event);

  // Returns current internal state.
  StickyKeyState current_state() const { return current_state_; }

 private:
  // Represents event type in Sticky Key context.
  enum KeyEventType {
    TARGET_MODIFIER_DOWN,  // The monitoring modifier key is down.
    TARGET_MODIFIER_UP,  // The monitoring modifier key is up.
    NORMAL_KEY_DOWN,  // The non modifier key is down.
    NORMAL_KEY_UP,  // The non modifier key is up.
    OTHER_MODIFIER_DOWN,  // The modifier key but not monitored key is down.
    OTHER_MODIFIER_UP,  // The modifier key but not monitored key is up.
  };

  // Translates |event| to sticky keys event type.
  KeyEventType TranslateKeyEvent(ui::KeyEvent* event);

  // Handles key event in DISABLED state.
  bool HandleDisabledState(ui::KeyEvent* event);

  // Handles key event in ENABLED state.
  bool HandleEnabledState(ui::KeyEvent* event);

  // Handles key event in LOCKED state.
  bool HandleLockedState(ui::KeyEvent* event);

  // Dispatches |event| to its target and then dispatch a key released event
  // for the modifier key. This function is required to ensure that the events
  // are sent in the correct order when disabling sticky key after a key/mouse
  // button press.
  void DispatchEventAndReleaseModifier(ui::Event* event);

  // Adds |modifier_flags_| to a native X11 event state mask.
  void AppendNativeEventMask(unsigned int* state);

  // Adds |modifier_flags_| into |event|.
  void AppendModifier(ui::KeyEvent* event);
  void AppendModifier(ui::MouseEvent* event);
  void AppendModifier(ui::ScrollEvent* event);

  // The modifier flag to be monitored and appended to events.
  const ui::EventFlags modifier_flag_;

  // The current sticky key status.
  StickyKeyState current_state_;

  // True if the received key event is sent by StickyKeyHandler.
  bool event_from_myself_;

  // True if we received the TARGET_MODIFIER_DOWN event while in the DISABLED
  // state but before we receive the TARGET_MODIFIER_UP event. Normal
  // shortcuts (eg. ctrl + t) during this time will prevent a transition to
  // the ENABLED state.
  bool preparing_to_enable_;

  // Tracks the scroll direction of the current scroll sequence. Sticky keys
  // stops modifying the scroll events of the sequence when the direction
  // changes. If no sequence is tracked, the value is 0.
  int scroll_delta_;

  // The modifier up key event to be sent on non modifier key on ENABLED state.
  scoped_ptr<ui::KeyEvent> modifier_up_event_;

  scoped_ptr<StickyKeysHandlerDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(StickyKeysHandler);
};

}  // namespace ash

#endif  // ASH_STICKY_KEYS_STICKY_KEYS_CONTROLLER_H_
