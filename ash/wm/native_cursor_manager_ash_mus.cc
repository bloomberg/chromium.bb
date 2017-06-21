// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/native_cursor_manager_ash_mus.h"

#include "ash/display/cursor_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/image_cursors.h"
#include "ui/base/layout.h"
#include "ui/wm/core/cursor_manager.h"

#if defined(USE_OZONE)
#include "ui/base/cursor/ozone/cursor_data_factory_ozone.h"
#endif

namespace ash {
namespace {

// We want to forward these things to the window tree client.

void SetCursorOnAllRootWindows(gfx::NativeCursor cursor) {
  ui::CursorData mojo_cursor;
  if (cursor.platform()) {
#if defined(USE_OZONE)
    mojo_cursor = ui::CursorDataFactoryOzone::GetCursorData(cursor.platform());
#else
    NOTIMPLEMENTED()
        << "Can't pass native platform cursors on non-ozone platforms";
    mojo_cursor = ui::CursorData(ui::CursorType::kPointer);
#endif
  } else {
    mojo_cursor = ui::CursorData(cursor.native_type());
  }

  // As the window manager, tell mus to use |mojo_cursor| everywhere. We do
  // this instead of trying to set per-window because otherwise we run into the
  // event targeting issue.
  ShellPort::Get()->SetGlobalOverrideCursor(mojo_cursor);

  Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->SetCursor(cursor);
}

void NotifyCursorVisibilityChange(bool visible) {
  // Communicate the cursor visibility state to the mus server.
  if (visible)
    ShellPort::Get()->ShowCursor();
  else
    ShellPort::Get()->HideCursor();

  // Communicate the cursor visibility change to our local root window objects.
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter)
    (*iter)->GetHost()->OnCursorVisibilityChanged(visible);

  Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->SetVisibility(visible);
}

void NotifyMouseEventsEnableStateChange(bool enabled) {
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter)
    (*iter)->GetHost()->dispatcher()->OnMouseEventsEnableStateChanged(enabled);
  // Mirror window never process events.
}

}  // namespace

NativeCursorManagerAshMus::NativeCursorManagerAshMus() {
#if defined(USE_OZONE)
  // If we're in a mus client, we aren't going to have all of ozone initialized
  // even though we're in an ozone build. All the hard coded USE_OZONE ifdefs
  // that handle cursor code expect that there will be a CursorFactoryOzone
  // instance. Partially initialize the ozone cursor internals here, like we
  // partially initialize other ozone subsystems in
  // ChromeBrowserMainExtraPartsViews.
  cursor_factory_ozone_ = base::MakeUnique<ui::CursorDataFactoryOzone>();
  image_cursors_ = base::MakeUnique<ui::ImageCursors>();
#endif
}

NativeCursorManagerAshMus::~NativeCursorManagerAshMus() = default;

void NativeCursorManagerAshMus::SetNativeCursorEnabled(bool enabled) {
  native_cursor_enabled_ = enabled;

  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  SetCursor(cursor_manager->GetCursor(), cursor_manager);
}

float NativeCursorManagerAshMus::GetScale() const {
  return image_cursors_->GetScale();
}

display::Display::Rotation NativeCursorManagerAshMus::GetRotation() const {
  return image_cursors_->GetRotation();
}

void NativeCursorManagerAshMus::SetDisplay(
    const display::Display& display,
    ::wm::NativeCursorManagerDelegate* delegate) {
  DCHECK(display.is_valid());
  // Use the platform's device scale factor instead of the display's, which
  // might have been adjusted for the UI scale.
  const float original_scale = Shell::Get()
                                   ->display_manager()
                                   ->GetDisplayInfo(display.id())
                                   .device_scale_factor();
  // And use the nearest resource scale factor.
  const float cursor_scale =
      ui::GetScaleForScaleFactor(ui::GetSupportedScaleFactor(original_scale));

  if (image_cursors_->SetDisplay(display, cursor_scale))
    SetCursor(delegate->GetCursor(), delegate);

  Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->SetDisplay(display);
}

void NativeCursorManagerAshMus::SetCursor(
    gfx::NativeCursor cursor,
    ::wm::NativeCursorManagerDelegate* delegate) {
  if (image_cursors_) {
    if (native_cursor_enabled_) {
      image_cursors_->SetPlatformCursor(&cursor);
    } else {
      gfx::NativeCursor invisible_cursor(ui::CursorType::kNone);
      image_cursors_->SetPlatformCursor(&invisible_cursor);
      if (cursor == ui::CursorType::kCustom) {
        // Fall back to the default pointer cursor for now. (crbug.com/476078)
        // TODO(oshima): support custom cursor.
        cursor = ui::CursorType::kPointer;
      } else {
        cursor.SetPlatformCursor(invisible_cursor.platform());
      }
    }
  }
  cursor.set_device_scale_factor(image_cursors_->GetScale());

  delegate->CommitCursor(cursor);

  if (delegate->IsCursorVisible())
    SetCursorOnAllRootWindows(cursor);
}

void NativeCursorManagerAshMus::SetVisibility(
    bool visible,
    ::wm::NativeCursorManagerDelegate* delegate) {
  delegate->CommitVisibility(visible);

  if (visible) {
    SetCursor(delegate->GetCursor(), delegate);
  } else {
    gfx::NativeCursor invisible_cursor(ui::CursorType::kNone);
    image_cursors_->SetPlatformCursor(&invisible_cursor);
    SetCursorOnAllRootWindows(invisible_cursor);
  }

  NotifyCursorVisibilityChange(visible);
}

void NativeCursorManagerAshMus::SetCursorSet(
    ui::CursorSetType cursor_set,
    ::wm::NativeCursorManagerDelegate* delegate) {
  // We can't just hand this off to ImageCursors like we do in the classic ash
  // case. We need to collaborate with the mus server to fully implement this.
  NOTIMPLEMENTED();
}

void NativeCursorManagerAshMus::SetMouseEventsEnabled(
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
