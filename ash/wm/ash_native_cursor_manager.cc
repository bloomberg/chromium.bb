// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/ash_native_cursor_manager.h"

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

AshNativeCursorManager::AshNativeCursorManager()
    : image_cursors_(new ImageCursors) {
}

AshNativeCursorManager::~AshNativeCursorManager() {
}

void AshNativeCursorManager::SetDeviceScaleFactor(
    float device_scale_factor,
    views::corewm::NativeCursorManagerDelegate* delegate) {
  if (image_cursors_->SetDeviceScaleFactor(device_scale_factor))
    SetCursor(delegate->GetCurrentCursor(), delegate);
}

void AshNativeCursorManager::SetCursor(
    gfx::NativeCursor cursor,
    views::corewm::NativeCursorManagerDelegate* delegate) {
  gfx::NativeCursor new_cursor = cursor;
  image_cursors_->SetPlatformCursor(&new_cursor);
  new_cursor.set_device_scale_factor(image_cursors_->GetDeviceScaleFactor());

  delegate->CommitCursor(new_cursor);

  if (delegate->GetCurrentVisibility())
    SetCursorOnAllRootWindows(new_cursor);
}

void AshNativeCursorManager::SetVisibility(
    bool visible,
    views::corewm::NativeCursorManagerDelegate* delegate) {
  delegate->CommitVisibility(visible);

  if (visible) {
    SetCursor(delegate->GetCurrentCursor(), delegate);
  } else {
    gfx::NativeCursor invisible_cursor(ui::kCursorNone);
    image_cursors_->SetPlatformCursor(&invisible_cursor);
    SetCursorOnAllRootWindows(invisible_cursor);
  }

  NotifyCursorVisibilityChange(visible);
}

void AshNativeCursorManager::SetMouseEventsEnabled(
    bool enabled,
    views::corewm::NativeCursorManagerDelegate* delegate) {
  delegate->CommitMouseEventsEnabled(enabled);

  if (enabled) {
    aura::Env::GetInstance()->set_last_mouse_location(
        disabled_cursor_location_);
  } else {
    disabled_cursor_location_ = aura::Env::GetInstance()->last_mouse_location();
    aura::Env::GetInstance()->set_last_mouse_location(
        gfx::Point(kDisabledCursorLocationX, kDisabledCursorLocationY));
  }

  SetVisibility(delegate->GetCurrentVisibility(), delegate);
  NotifyMouseEventsEnableStateChange(enabled);
}

}  // namespace ash
