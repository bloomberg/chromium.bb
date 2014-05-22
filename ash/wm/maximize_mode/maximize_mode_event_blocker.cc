// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_event_blocker.h"

#include "ash/shell.h"
#include "ash/wm/maximize_mode/internal_input_device_list.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_targeter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/point.h"

#if defined(USE_X11)
#include "ash/wm/maximize_mode/internal_input_device_list_x11.h"
#endif

namespace ash {

namespace {

// Event targeter to prevent delivery of mouse and touchpad events while
// maximize mode is active. Other events such as touch are passed on to the
// default targeter.
class BlockKeyboardAndTouchpadTargeter : public ui::EventTargeter {
 public:
  BlockKeyboardAndTouchpadTargeter(
      aura::Window* root_window,
      MaximizeModeEventBlocker* event_blocker);
  virtual ~BlockKeyboardAndTouchpadTargeter();

  // Sets the default targeter to use when the event is not being blocked.
  void SetDefaultTargeter(EventTargeter* default_targeter);

  // Overridden from ui::EventTargeter:
  virtual ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                              ui::Event* event) OVERRIDE;

 private:
  // A weak pointer to the root window on which this targeter will be set. The
  // root window owns this targeter.
  aura::Window* root_window_;

  // A weak pointer to the event blocker which owns the scoped targeter owning
  // this targeter.
  MaximizeModeEventBlocker* event_blocker_;

  // A weak pointer to the targeter this targeter is wrapping. The
  // default_targeter is owned by the ScopedWindowTargeter which will be valid
  // as long as this targeter is alive.
  ui::EventTargeter* default_targeter_;

  // The last known mouse location to lock the cursor in place to when events
  // come from the internal touchpad.
  gfx::Point last_mouse_location_;

  DISALLOW_COPY_AND_ASSIGN(BlockKeyboardAndTouchpadTargeter);
};

BlockKeyboardAndTouchpadTargeter::BlockKeyboardAndTouchpadTargeter(
    aura::Window* root_window,
    MaximizeModeEventBlocker* event_blocker)
    : root_window_(root_window),
      event_blocker_(event_blocker),
      default_targeter_(NULL),
      last_mouse_location_(root_window->GetHost()->dispatcher()->
          GetLastMouseLocationInRoot()) {
}

BlockKeyboardAndTouchpadTargeter::~BlockKeyboardAndTouchpadTargeter() {
}

void BlockKeyboardAndTouchpadTargeter::SetDefaultTargeter(
    ui::EventTargeter* default_targeter) {
  default_targeter_ = default_targeter;
}

ui::EventTarget* BlockKeyboardAndTouchpadTargeter::FindTargetForEvent(
    ui::EventTarget* root,
    ui::Event* event) {
  bool internal_device = event_blocker_->internal_devices() &&
      event_blocker_->internal_devices()->IsEventFromInternalDevice(event);
  if (event->IsMouseEvent()) {
    if (internal_device) {
      // The cursor movement is handled at a lower level which is not blocked.
      // Move the mouse cursor back to its last known location resulting from
      // an external mouse to prevent the internal touchpad from moving it.
      root_window_->GetHost()->MoveCursorToHostLocation(
          last_mouse_location_);
      return NULL;
    } else {
      // Track the last location seen from an external mouse event.
      last_mouse_location_ =
          static_cast<ui::MouseEvent*>(event)->root_location();
      root_window_->GetHost()->ConvertPointToHost(&last_mouse_location_);
    }
  } else if (internal_device && (event->IsMouseWheelEvent() ||
                                 event->IsScrollEvent())) {
    return NULL;
  } else if (event->IsKeyEvent() && event->HasNativeEvent()) {
    // TODO(flackr): Disable events only from the internal keyboard device
    // when we begin using XI2 events for keyboard events
    // (http://crbug.com/368750) and can tell which device the event is
    // coming from, http://crbug.com/362881.
    // TODO(bruthig): Fix this to block rewritten volume keys
    // (i.e. F9 and F10)  from the device's keyboard. https://crbug.com/368669
    ui::KeyEvent* key_event = static_cast<ui::KeyEvent*>(event);
    if (key_event->key_code() != ui::VKEY_VOLUME_DOWN &&
        key_event->key_code() != ui::VKEY_VOLUME_UP
#if defined(OS_CHROMEOS)
      && key_event->key_code() != ui::VKEY_POWER
#endif
        ) {
      return NULL;
    }
  }
  return default_targeter_->FindTargetForEvent(root, event);
}

}  // namespace

MaximizeModeEventBlocker::MaximizeModeEventBlocker()
#if defined(USE_X11)
    : internal_devices_(new InternalInputDeviceListX11)
#endif
    {
  Shell::GetInstance()->AddShellObserver(this);

  // Hide the cursor as mouse events will be blocked.
  aura::client::CursorClient* cursor_client_ =
      aura::client::GetCursorClient(Shell::GetTargetRootWindow());
  if (cursor_client_)
    cursor_client_->HideCursor();

  // Block keyboard and mouse events on all existing and new root windows for
  // the lifetime of this class.
  aura::Window::Windows root_windows(Shell::GetAllRootWindows());
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    AddEventTargeterOn(*iter);
  }
}

MaximizeModeEventBlocker::~MaximizeModeEventBlocker() {
  Shell::GetInstance()->RemoveShellObserver(this);
}

void MaximizeModeEventBlocker::OnRootWindowAdded(aura::Window* root_window) {
  AddEventTargeterOn(root_window);
}

void MaximizeModeEventBlocker::AddEventTargeterOn(
    aura::Window* root_window) {
  BlockKeyboardAndTouchpadTargeter* targeter =
      new BlockKeyboardAndTouchpadTargeter(root_window, this);
  aura::ScopedWindowTargeter* scoped_targeter = new aura::ScopedWindowTargeter(
      root_window, scoped_ptr<ui::EventTargeter>(targeter));
  targeter->SetDefaultTargeter(scoped_targeter->old_targeter());
  targeters_.push_back(scoped_targeter);
}

}  // namespace ash
