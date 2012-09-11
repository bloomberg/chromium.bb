// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/cursor_manager.h"

#include "ash/wm/cursor_delegate.h"
#include "ash/wm/image_cursors.h"
#include "base/logging.h"
#include "ui/aura/env.h"
#include "ui/base/cursor/cursor.h"

namespace ash {

CursorManager::CursorManager()
    : delegate_(NULL),
      cursor_lock_count_(0),
      did_cursor_change_(false),
      cursor_to_set_on_unlock_(0),
      cursor_visible_(true),
      current_cursor_(ui::kCursorNone),
      image_cursors_(new ImageCursors) {
}

CursorManager::~CursorManager() {
}

void CursorManager::LockCursor() {
  cursor_lock_count_++;
}

void CursorManager::UnlockCursor() {
  cursor_lock_count_--;
  DCHECK_GE(cursor_lock_count_, 0);
  if (cursor_lock_count_ == 0) {
    if (did_cursor_change_) {
      did_cursor_change_ = false;
      if (delegate_)
        delegate_->SetCursor(cursor_to_set_on_unlock_);
    }
    did_cursor_change_ = false;
    cursor_to_set_on_unlock_ = gfx::kNullCursor;
  }
}

void CursorManager::SetCursor(gfx::NativeCursor cursor) {
  if (cursor_lock_count_ == 0) {
    if (delegate_)
      SetCursorInternal(cursor);
  } else {
    cursor_to_set_on_unlock_ = cursor;
    did_cursor_change_ = true;
  }
}

void CursorManager::ShowCursor(bool show) {
  cursor_visible_ = show;
  if (delegate_)
    delegate_->ShowCursor(show);
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

void CursorManager::SetCursorInternal(gfx::NativeCursor cursor) {
  DCHECK(delegate_);
  current_cursor_ = cursor;
  image_cursors_->SetPlatformCursor(&current_cursor_);
  current_cursor_.set_device_scale_factor(
      image_cursors_->GetDeviceScaleFactor());
  delegate_->SetCursor(current_cursor_);
}

}  // namespace ash
