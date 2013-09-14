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

UserInputMonitor::UserInputMonitor()
    : monitoring_mouse_(false), key_press_counter_references_(0) {}

UserInputMonitor::~UserInputMonitor() {
  DCHECK(!monitoring_mouse_);
  DCHECK(!key_press_counter_references_);
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

void UserInputMonitor::EnableKeyPressMonitoring() {
  base::AutoLock auto_lock(lock_);
  ++key_press_counter_references_;
  if (key_press_counter_references_ == 1) {
    StartKeyboardMonitoring();
    DVLOG(2) << "Started keyboard monitoring.";
  }
}

void UserInputMonitor::DisableKeyPressMonitoring() {
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(key_press_counter_references_, 0u);
  --key_press_counter_references_;
  if (key_press_counter_references_ == 0) {
    StopKeyboardMonitoring();
    DVLOG(2) << "Stopped keyboard monitoring.";
  }
}

void UserInputMonitor::OnMouseEvent(const SkIPoint& position) {
  base::AutoLock auto_lock(lock_);
  if (!monitoring_mouse_)
    return;
  FOR_EACH_OBSERVER(
      MouseEventListener, mouse_listeners_, OnMouseMoved(position));
}

}  // namespace media
