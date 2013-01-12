// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/cursor_manager.h"

#include "ash/shell.h"
#include "ash/wm/image_cursors.h"
#include "base/logging.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/base/cursor/cursor.h"

namespace  {

// The coordinate of the cursor used when the mouse events are disabled.
const int kDisabledCursorLocationX = -10000;
const int kDisabledCursorLocationY = -10000;

void SetCursorOnAllRootWindows(gfx::NativeCursor cursor) {
  ash::Shell::RootWindowList root_windows =
      ash::Shell::GetInstance()->GetAllRootWindows();
  for (ash::Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter)
    (*iter)->SetCursor(cursor);
}

void NotifyCursorVisibilityChange(bool visible) {
  ash::Shell::RootWindowList root_windows =
      ash::Shell::GetInstance()->GetAllRootWindows();
  for (ash::Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter)
    (*iter)->OnCursorVisibilityChanged(visible);
}

void NotifyMouseEventsEnableStateChange(bool enabled) {
  ash::Shell::RootWindowList root_windows =
      ash::Shell::GetInstance()->GetAllRootWindows();
  for (ash::Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter)
    (*iter)->OnMouseEventsEnableStateChanged(enabled);
}

}  // namespace

namespace ash {
namespace internal {

// Represents the cursor state which is composed of cursor type, visibility, and
// mouse events enable state. When mouse events are disabled, the cursor is
// always invisible.
class CursorState {
 public:
  CursorState()
      : cursor_(ui::kCursorNone),
        visible_(true),
        mouse_events_enabled_(true),
        visible_on_mouse_events_enabled_(true) {
  }

  gfx::NativeCursor cursor() const { return cursor_; }
  void set_cursor(gfx::NativeCursor cursor) { cursor_ = cursor; }

  bool visible() const { return visible_; }
  void SetVisible(bool visible) {
    if (mouse_events_enabled_)
      visible_ = visible;
    // Ignores the call when mouse events disabled.
  }

  bool mouse_events_enabled() const { return mouse_events_enabled_; }
  void SetMouseEventsEnabled(bool enabled) {
    if (mouse_events_enabled_ == enabled)
      return;
    mouse_events_enabled_ = enabled;

    // Restores the visibility when mouse events are enabled.
    if (enabled) {
      visible_ = visible_on_mouse_events_enabled_;
    } else {
      visible_on_mouse_events_enabled_ = visible_;
      visible_ = false;
    }
  }

 private:
  gfx::NativeCursor cursor_;
  bool visible_;
  bool mouse_events_enabled_;

  // The visibility to set when mouse events are enabled.
  bool visible_on_mouse_events_enabled_;

  DISALLOW_COPY_AND_ASSIGN(CursorState);
};

}  // namespace internal

CursorManager::CursorManager()
    : cursor_lock_count_(0),
      current_state_(new internal::CursorState),
      state_on_unlock_(new internal::CursorState),
      image_cursors_(new ImageCursors) {
}

CursorManager::~CursorManager() {
}

void CursorManager::SetCursor(gfx::NativeCursor cursor) {
  state_on_unlock_->set_cursor(cursor);
  if (cursor_lock_count_ == 0 &&
      GetCurrentCursor() != state_on_unlock_->cursor()) {
    SetCursorInternal(state_on_unlock_->cursor());
  }
}

void CursorManager::ShowCursor() {
  state_on_unlock_->SetVisible(true);
  if (cursor_lock_count_ == 0 &&
      IsCursorVisible() != state_on_unlock_->visible()) {
    SetCursorVisibility(state_on_unlock_->visible());
  }
}

void CursorManager::HideCursor() {
  state_on_unlock_->SetVisible(false);
  if (cursor_lock_count_ == 0 &&
      IsCursorVisible() != state_on_unlock_->visible()) {
    SetCursorVisibility(state_on_unlock_->visible());
  }
}

bool CursorManager::IsCursorVisible() const {
  return current_state_->visible();
}

void CursorManager::EnableMouseEvents() {
  state_on_unlock_->SetMouseEventsEnabled(true);
  if (cursor_lock_count_ == 0 &&
      IsMouseEventsEnabled() != state_on_unlock_->mouse_events_enabled()) {
    SetMouseEventsEnabled(state_on_unlock_->mouse_events_enabled());
  }
}

void CursorManager::DisableMouseEvents() {
  state_on_unlock_->SetMouseEventsEnabled(false);
  if (cursor_lock_count_ == 0 &&
      IsMouseEventsEnabled() != state_on_unlock_->mouse_events_enabled()) {
    SetMouseEventsEnabled(state_on_unlock_->mouse_events_enabled());
  }
}

bool CursorManager::IsMouseEventsEnabled() const {
  return current_state_->mouse_events_enabled();
}

void CursorManager::SetDeviceScaleFactor(float device_scale_factor) {
  if (image_cursors_->SetDeviceScaleFactor(device_scale_factor))
    SetCursorInternal(GetCurrentCursor());
}

void CursorManager::LockCursor() {
  cursor_lock_count_++;
}

void CursorManager::UnlockCursor() {
  cursor_lock_count_--;
  DCHECK_GE(cursor_lock_count_, 0);
  if (cursor_lock_count_ > 0)
    return;

  if (GetCurrentCursor() != state_on_unlock_->cursor())
    SetCursorInternal(state_on_unlock_->cursor());
  if (IsMouseEventsEnabled() != state_on_unlock_->mouse_events_enabled())
    SetMouseEventsEnabled(state_on_unlock_->mouse_events_enabled());
  if (IsCursorVisible() != state_on_unlock_->visible())
    SetCursorVisibility(state_on_unlock_->visible());
}

void CursorManager::SetCursorInternal(gfx::NativeCursor cursor) {
  gfx::NativeCursor new_cursor = cursor;
  image_cursors_->SetPlatformCursor(&new_cursor);
  new_cursor.set_device_scale_factor(image_cursors_->GetDeviceScaleFactor());
  current_state_->set_cursor(new_cursor);

  if (IsCursorVisible())
    SetCursorOnAllRootWindows(GetCurrentCursor());
}

void CursorManager::SetCursorVisibility(bool visible) {
  current_state_->SetVisible(visible);

  if (visible) {
    SetCursorInternal(GetCurrentCursor());
  } else {
    gfx::NativeCursor invisible_cursor(ui::kCursorNone);
    image_cursors_->SetPlatformCursor(&invisible_cursor);
    SetCursorOnAllRootWindows(invisible_cursor);
  }

  NotifyCursorVisibilityChange(visible);
}

void CursorManager::SetMouseEventsEnabled(bool enabled) {
  current_state_->SetMouseEventsEnabled(enabled);

  if (enabled) {
    aura::Env::GetInstance()->set_last_mouse_location(
        disabled_cursor_location_);
  } else {
    disabled_cursor_location_ = aura::Env::GetInstance()->last_mouse_location();
    aura::Env::GetInstance()->set_last_mouse_location(
        gfx::Point(kDisabledCursorLocationX, kDisabledCursorLocationY));
  }

  SetCursorVisibility(current_state_->visible());
  NotifyMouseEventsEnableStateChange(enabled);
}

gfx::NativeCursor CursorManager::GetCurrentCursor() const {
  return current_state_->cursor();
}

}  // namespace ash
