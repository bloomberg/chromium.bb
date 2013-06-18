// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_STICKY_KEYS_H_
#define ASH_WM_STICKY_KEYS_H_

#include "ash/ash_export.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/events/event_constants.h"

namespace ui {
class KeyEvent;
}  // namespace ui

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class StickyKeysHandler;

// StickyKeys is an accessibility feature for users to be able to compose
// key event with modifier keys without simultaneous key press event. Instead,
// they can compose modified key events separately pressing each of the keys
// involved.
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
// In the case of Chrome OS, StickyKeys supports Shift,Alt,Ctrl modifiers. Each
// handling or state is performed independently.
//
// StickyKeys is disabled by default.
class ASH_EXPORT StickyKeys {
 public:
  StickyKeys();
  ~StickyKeys();

  // Handles keyboard event. Returns true if Sticky key consumes keyboard event.
  bool HandleKeyEvent(ui::KeyEvent* event);

 private:
  // Sticky key handlers.
  scoped_ptr<StickyKeysHandler> shift_sticky_key_;
  scoped_ptr<StickyKeysHandler> alt_sticky_key_;
  scoped_ptr<StickyKeysHandler> ctrl_sticky_key_;

  DISALLOW_COPY_AND_ASSIGN(StickyKeys);
};

// StickyKeysHandler handles key event and performs StickyKeys for specific
// modifier keys. If monitored keyboard events are recieved, StickyKeysHandler
// changes internal state. If non modifier keyboard events are received,
// StickyKeysHandler will append modifier based on internal state. For other
// events, StickyKeysHandler does nothing.
//
// The DISABLED state is default state and any incomming non modifier keyboard
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
  };
  // Represents Sticky Key state.
  enum StickyKeyState {
    // The sticky key is disabled. Incomming non modifier key events are not
    // affected.
    DISABLED,
    // The sticky key is enabled. Incomming non modifier key down events are
    // modified with |modifier_flag_|. After that, sticky key state become
    // DISABLED.
    ENABLED,
    // The sticky key is locked. Incomming non modifier key down events are
    // modified with |modifier_flag_|.
    LOCKED,
  };

  // This class takes an ownership of |delegate|.
  StickyKeysHandler(ui::EventFlags modifier_flag,
                    StickyKeysHandlerDelegate* delegate);
  ~StickyKeysHandler();

  // Handles key event. Returns true if key is consumed.
  bool HandleKeyEvent(ui::KeyEvent* event);

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

  // Adds |modifier_flags_| into |event|.
  void AppendModifier(ui::KeyEvent* event);

  // The modifier flag to be monitored and appended.
  const ui::EventFlags modifier_flag_;

  // The current sticky key status.
  StickyKeyState current_state_;

  // True if the received key event is sent by StickyKeyHandler.
  bool keyevent_from_myself_;

  // The modifier up key event to be sent on non modifier key on ENABLED state.
  scoped_ptr<ui::KeyEvent> modifier_up_event_;

  scoped_ptr<StickyKeysHandlerDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(StickyKeysHandler);
};

}  // namespace ash

#endif  // ASH_WM_STICKY_KEYS_H_
