// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/cursor_manager.h"

#include "ash/shell.h"
#include "ash/wm/image_cursors.h"
#include "base/logging.h"
#include "ui/aura/root_window.h"
#include "ui/base/cursor/cursor.h"

namespace  {

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

}  // namespace

namespace ash {

CursorManager::CursorManager()
    : cursor_lock_count_(0),
      did_cursor_change_(false),
      cursor_to_set_on_unlock_(0),
      did_visibility_change_(false),
      show_on_unlock_(true),
      cursor_visible_(true),
      current_cursor_(ui::kCursorNone),
      image_cursors_(new ImageCursors) {
}

CursorManager::~CursorManager() {
}

void CursorManager::SetCursor(gfx::NativeCursor cursor) {
  if (cursor_lock_count_ == 0) {
    SetCursorInternal(cursor);
  } else {
    cursor_to_set_on_unlock_ = cursor;
    did_cursor_change_ = true;
  }
}

void CursorManager::ShowCursor(bool show) {
  if (cursor_lock_count_ == 0) {
    ShowCursorInternal(show);
  } else {
    show_on_unlock_ = show;
    did_visibility_change_ = true;
  }
}

bool CursorManager::IsCursorVisible() const {
  return cursor_visible_;
}

void CursorManager::SetDeviceScaleFactor(float device_scale_factor) {
  if (image_cursors_->GetDeviceScaleFactor() == device_scale_factor)
    return;
  image_cursors_->SetDeviceScaleFactor(device_scale_factor);
  SetCursorInternal(current_cursor_);
}

void CursorManager::LockCursor() {
  cursor_lock_count_++;
}

void CursorManager::UnlockCursor() {
  cursor_lock_count_--;
  DCHECK_GE(cursor_lock_count_, 0);
  if (cursor_lock_count_ > 0)
    return;

  if (did_cursor_change_)
    SetCursorInternal(cursor_to_set_on_unlock_);
  did_cursor_change_ = false;
  cursor_to_set_on_unlock_ = gfx::kNullCursor;

  if (did_visibility_change_)
    ShowCursorInternal(show_on_unlock_);
  did_visibility_change_ = false;
}

void CursorManager::SetCursorInternal(gfx::NativeCursor cursor) {
  current_cursor_ = cursor;
  image_cursors_->SetPlatformCursor(&current_cursor_);
  current_cursor_.set_device_scale_factor(
      image_cursors_->GetDeviceScaleFactor());

  if (cursor_visible_)
    SetCursorOnAllRootWindows(current_cursor_);
}

void CursorManager::ShowCursorInternal(bool show) {
  if (cursor_visible_ == show)
    return;

  cursor_visible_ = show;

  if (show) {
    SetCursorInternal(current_cursor_);
  } else {
    gfx::NativeCursor invisible_cursor(ui::kCursorNone);
    image_cursors_->SetPlatformCursor(&invisible_cursor);
    SetCursorOnAllRootWindows(invisible_cursor);
  }

  NotifyCursorVisibilityChange(show);
}

}  // namespace ash
