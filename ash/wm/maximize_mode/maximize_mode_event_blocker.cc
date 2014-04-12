// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_event_blocker.h"

#include "ash/shell.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/events/event_targeter.h"

namespace ash {

namespace {

// Event targeter to prevent delivery of mouse and touchpad events while
// maximize mode is active. Other events such as touch are passed on to the
// default targeter.
// TODO(flackr): This should only stop events from the internal keyboard and
// touchpad.
class BlockKeyboardAndTouchpadTargeter : public ui::EventTargeter {
 public:
  BlockKeyboardAndTouchpadTargeter();
  virtual ~BlockKeyboardAndTouchpadTargeter();

  // Sets the default targeter to use when the event is not being blocked.
  void SetDefaultTargeter(EventTargeter* default_targeter);

  // Overridden from ui::EventTargeter:
  virtual ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                              ui::Event* event) OVERRIDE;

 private:
  // A weak pointer to the targeter this targeter is wrapping. The
  // default_targeter is owned by the ScopedWindowTargeter which will be valid
  // as long as this targeter is alive.
  ui::EventTargeter* default_targeter_;

  DISALLOW_COPY_AND_ASSIGN(BlockKeyboardAndTouchpadTargeter);
};

BlockKeyboardAndTouchpadTargeter::BlockKeyboardAndTouchpadTargeter()
    : default_targeter_(NULL) {
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
  if (event->HasNativeEvent() && (event->IsMouseEvent() || event->IsKeyEvent()))
    return NULL;
  return default_targeter_->FindTargetForEvent(root, event);
}

}  // namespace

MaximizeModeEventBlocker::MaximizeModeEventBlocker() {
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
      new BlockKeyboardAndTouchpadTargeter();
  aura::ScopedWindowTargeter* scoped_targeter = new aura::ScopedWindowTargeter(
      root_window, scoped_ptr<ui::EventTargeter>(targeter));
  targeter->SetDefaultTargeter(scoped_targeter->old_targeter());
  targeters_.push_back(scoped_targeter);
}

}  // namespace ash
