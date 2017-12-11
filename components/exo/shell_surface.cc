// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, public:

ShellSurface::ShellSurface(Surface* surface,
                           const gfx::Point& origin,
                           bool activatable,
                           bool can_minimize,
                           int container)
    : ShellSurfaceBase(surface, origin, activatable, can_minimize, container) {}

ShellSurface::ShellSurface(Surface* surface)
    : ShellSurfaceBase(surface,
                       gfx::Point(),
                       true,
                       true,
                       ash::kShellWindowId_DefaultContainer) {}

ShellSurface::~ShellSurface() {
}

void ShellSurface::SetParent(ShellSurface* parent) {
  TRACE_EVENT1("exo", "ShellSurface::SetParent", "parent",
               parent ? base::UTF16ToASCII(parent->title_) : "null");

  SetParentWindow(parent ? parent->GetWidget()->GetNativeWindow() : nullptr);
}

void ShellSurface::Maximize() {
  TRACE_EVENT0("exo", "ShellSurface::Maximize");

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_MAXIMIZED);

  // Note: This will ask client to configure its surface even if already
  // maximized.
  ScopedConfigure scoped_configure(this, true);
  widget_->Maximize();
}

void ShellSurface::Minimize() {
  TRACE_EVENT0("exo", "ShellSurface::Minimize");

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_MINIMIZED);

  // Note: This will ask client to configure its surface even if already
  // minimized.
  ScopedConfigure scoped_configure(this, true);
  widget_->Minimize();
}

void ShellSurface::Restore() {
  TRACE_EVENT0("exo", "ShellSurface::Restore");

  if (!widget_)
    return;

  // Note: This will ask client to configure its surface even if not already
  // maximized or minimized.
  ScopedConfigure scoped_configure(this, true);
  widget_->Restore();
}

void ShellSurface::SetFullscreen(bool fullscreen) {
  TRACE_EVENT1("exo", "ShellSurface::SetFullscreen", "fullscreen", fullscreen);

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_FULLSCREEN);

  // Note: This will ask client to configure its surface even if fullscreen
  // state doesn't change.
  ScopedConfigure scoped_configure(this, true);
  widget_->SetFullscreen(fullscreen);
}

void ShellSurface::DisableMovement() {
  movement_disabled_ = true;

  if (widget_)
    widget_->set_movement_disabled(true);
}

void ShellSurface::Resize(int component) {
  TRACE_EVENT1("exo", "ShellSurface::Resize", "component", component);

  if (!widget_)
    return;

  AttemptToStartDrag(component);
}

void ShellSurface::InitializeWindowState(ash::wm::WindowState* window_state) {
  window_state->set_allow_set_bounds_direct(false);
  widget_->set_movement_disabled(movement_disabled_);
  window_state->set_ignore_keyboard_bounds_change(movement_disabled_);
}

}  // namespace exo
