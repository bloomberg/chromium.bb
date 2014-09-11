// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/ash_native_cursor_manager.h"

#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/image_cursors.h"
#include "ui/base/layout.h"

namespace ash {
namespace  {

void SetCursorOnAllRootWindows(gfx::NativeCursor cursor) {
  aura::Window::Windows root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter)
    (*iter)->GetHost()->SetCursor(cursor);
#if defined(OS_CHROMEOS)
  Shell::GetInstance()->display_controller()->
      cursor_window_controller()->SetCursor(cursor);
#endif
}

void NotifyCursorVisibilityChange(bool visible) {
  aura::Window::Windows root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter)
    (*iter)->GetHost()->OnCursorVisibilityChanged(visible);
#if defined(OS_CHROMEOS)
  Shell::GetInstance()->display_controller()->cursor_window_controller()->
      SetVisibility(visible);
#endif
}

void NotifyMouseEventsEnableStateChange(bool enabled) {
  aura::Window::Windows root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter)
    (*iter)->GetHost()->dispatcher()->OnMouseEventsEnableStateChanged(enabled);
  // Mirror window never process events.
}

}  // namespace

AshNativeCursorManager::AshNativeCursorManager()
    : native_cursor_enabled_(true),
      image_cursors_(new ui::ImageCursors) {
}

AshNativeCursorManager::~AshNativeCursorManager() {
}


void AshNativeCursorManager::SetNativeCursorEnabled(bool enabled) {
  native_cursor_enabled_ = enabled;

  ::wm::CursorManager* cursor_manager =
      Shell::GetInstance()->cursor_manager();
  SetCursor(cursor_manager->GetCursor(), cursor_manager);
}

void AshNativeCursorManager::SetDisplay(
    const gfx::Display& display,
    ::wm::NativeCursorManagerDelegate* delegate) {
  DCHECK(display.is_valid());
  // Use the platform's device scale factor instead of the display's, which
  // might have been adjusted for the UI scale.
  const float original_scale = Shell::GetInstance()->display_manager()->
      GetDisplayInfo(display.id()).device_scale_factor();
#if defined(OS_CHROMEOS)
  // And use the nearest resource scale factor.
  const float cursor_scale = ui::GetScaleForScaleFactor(
      ui::GetSupportedScaleFactor(original_scale));
#else
  // TODO(oshima): crbug.com/143619
  const float cursor_scale = original_scale;
#endif
  if (image_cursors_->SetDisplay(display, cursor_scale))
    SetCursor(delegate->GetCursor(), delegate);
#if defined(OS_CHROMEOS)
  Shell::GetInstance()->display_controller()->cursor_window_controller()->
      SetDisplay(display);
#endif
}

void AshNativeCursorManager::SetCursor(
    gfx::NativeCursor cursor,
    ::wm::NativeCursorManagerDelegate* delegate) {
  if (native_cursor_enabled_) {
    image_cursors_->SetPlatformCursor(&cursor);
  } else {
    gfx::NativeCursor invisible_cursor(ui::kCursorNone);
    image_cursors_->SetPlatformCursor(&invisible_cursor);
    if (cursor == ui::kCursorCustom) {
      cursor = invisible_cursor;
    } else {
      cursor.SetPlatformCursor(invisible_cursor.platform());
    }
  }
  cursor.set_device_scale_factor(image_cursors_->GetScale());

  delegate->CommitCursor(cursor);

  if (delegate->IsCursorVisible())
    SetCursorOnAllRootWindows(cursor);
}

void AshNativeCursorManager::SetCursorSet(
    ui::CursorSetType cursor_set,
    ::wm::NativeCursorManagerDelegate* delegate) {
  image_cursors_->SetCursorSet(cursor_set);
  delegate->CommitCursorSet(cursor_set);

  // Sets the cursor to reflect the scale change immediately.
  if (delegate->IsCursorVisible())
    SetCursor(delegate->GetCursor(), delegate);

#if defined(OS_CHROMEOS)
  Shell::GetInstance()->display_controller()->cursor_window_controller()->
      SetCursorSet(cursor_set);
#endif
}

void AshNativeCursorManager::SetVisibility(
    bool visible,
    ::wm::NativeCursorManagerDelegate* delegate) {
  delegate->CommitVisibility(visible);

  if (visible) {
    SetCursor(delegate->GetCursor(), delegate);
  } else {
    gfx::NativeCursor invisible_cursor(ui::kCursorNone);
    image_cursors_->SetPlatformCursor(&invisible_cursor);
    SetCursorOnAllRootWindows(invisible_cursor);
  }

  NotifyCursorVisibilityChange(visible);
}

void AshNativeCursorManager::SetMouseEventsEnabled(
    bool enabled,
    ::wm::NativeCursorManagerDelegate* delegate) {
  delegate->CommitMouseEventsEnabled(enabled);

  if (enabled) {
    aura::Env::GetInstance()->set_last_mouse_location(
        disabled_cursor_location_);
  } else {
    disabled_cursor_location_ = aura::Env::GetInstance()->last_mouse_location();
  }

  SetVisibility(delegate->IsCursorVisible(), delegate);
  NotifyMouseEventsEnableStateChange(enabled);
}

}  // namespace ash
