// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/user_input_monitor.h"

#include "third_party/skia/include/core/SkPoint.h"

namespace media {

#ifdef DISABLE_USER_INPUT_MONITOR
scoped_ptr<UserInputMonitor> UserInputMonitor::Create(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner) {
  return scoped_ptr<UserInputMonitor>();
}
#endif  // DISABLE_USER_INPUT_MONITOR

UserInputMonitor::~UserInputMonitor() {
  DCHECK(!monitoring_mouse_);
  DCHECK(!monitoring_keyboard_);
}

void UserInputMonitor::AddMouseListener(MouseEventListener* listener) {
  base::AutoLock auto_lock(lock_);
  mouse_listeners_.AddObserver(listener);
  if (!monitoring_mouse_) {
    StartMouseMonitoring();
    monitoring_mouse_ = true;
    DVLOG(2) << "Started mouse monitoring.";
  }
}
void UserInputMonitor::RemoveMouseListener(MouseEventListener* listener) {
  base::AutoLock auto_lock(lock_);
  mouse_listeners_.RemoveObserver(listener);
  if (!mouse_listeners_.might_have_observers()) {
    StopMouseMonitoring();
    monitoring_mouse_ = false;
    DVLOG(2) << "Stopped mouse monitoring.";
  }
}
void UserInputMonitor::AddKeyStrokeListener(KeyStrokeListener* listener) {
  base::AutoLock auto_lock(lock_);
  key_stroke_listeners_.AddObserver(listener);
  if (!monitoring_keyboard_) {
    StartKeyboardMonitoring();
    monitoring_keyboard_ = true;
    DVLOG(2) << "Started keyboard monitoring.";
  }
}
void UserInputMonitor::RemoveKeyStrokeListener(KeyStrokeListener* listener) {
  base::AutoLock auto_lock(lock_);
  key_stroke_listeners_.RemoveObserver(listener);
  if (!key_stroke_listeners_.might_have_observers()) {
    StopKeyboardMonitoring();
    monitoring_keyboard_ = false;
    DVLOG(2) << "Stopped keyboard monitoring.";
  }
}

UserInputMonitor::UserInputMonitor()
    : monitoring_mouse_(false), monitoring_keyboard_(false) {}

void UserInputMonitor::OnMouseEvent(const SkIPoint& position) {
  base::AutoLock auto_lock(lock_);
  FOR_EACH_OBSERVER(
      MouseEventListener, mouse_listeners_, OnMouseMoved(position));
}

void UserInputMonitor::OnKeyboardEvent(ui::EventType event,
                                       ui::KeyboardCode key_code) {
  base::AutoLock auto_lock(lock_);
  // Updates the pressed keys and maybe notifies the key_stroke_listeners_.
  if (event == ui::ET_KEY_PRESSED) {
    if (pressed_keys_.find(key_code) != pressed_keys_.end())
      return;
    pressed_keys_.insert(key_code);
    DVLOG(6) << "Key stroke detected.";
    FOR_EACH_OBSERVER(KeyStrokeListener, key_stroke_listeners_, OnKeyStroke());
  } else {
    DCHECK_EQ(ui::ET_KEY_RELEASED, event);
    DCHECK(pressed_keys_.find(key_code) != pressed_keys_.end());
    pressed_keys_.erase(key_code);
  }
}

}  // namespace media
