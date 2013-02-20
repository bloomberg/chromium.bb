// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panel_window_resizer.h"

#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/cursor_manager.h"
#include "ash/wm/panel_layout_manager.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {
const int kPanelSnapToLauncherDistance = 30;
}  // namespace

PanelWindowResizer::~PanelWindowResizer() {
  ash::Shell::GetInstance()->cursor_manager()->UnlockCursor();
}

// static
PanelWindowResizer*
PanelWindowResizer::Create(aura::Window* window,
                             const gfx::Point& location,
                             int window_component) {
  Details details(window, location, window_component);
  return details.is_resizable ? new PanelWindowResizer(details) : NULL;
}

void PanelWindowResizer::Drag(const gfx::Point& location, int event_flags) {
  gfx::Rect bounds(CalculateBoundsForDrag(details_, location));
  if (bounds != details_.window->bounds()) {
    if (!did_move_or_resize_) {
      if (!details_.restore_bounds.IsEmpty())
        ClearRestoreBounds(details_.window);
      StartedDragging();
    }
    did_move_or_resize_ = true;
    should_attach_ = AttachToLauncher(&bounds);
    details_.window->SetBounds(bounds);
  }
}

void PanelWindowResizer::CompleteDrag(int event_flags) {
  if (should_attach_ != was_attached_) {
    details_.window->SetProperty(internal::kPanelAttachedKey, should_attach_);
    details_.window->SetDefaultParentByRootWindow(
            details_.window->GetRootWindow(),
            details_.window->bounds());
  }
  FinishDragging();
}

void PanelWindowResizer::RevertDrag() {
  if (!did_move_or_resize_)
    return;

  details_.window->SetBounds(details_.initial_bounds_in_parent);

  if (!details_.restore_bounds.IsEmpty())
    SetRestoreBoundsInScreen(details_.window, details_.restore_bounds);
  FinishDragging();
}

aura::Window* PanelWindowResizer::GetTarget() {
  return details_.window;
}

PanelWindowResizer::PanelWindowResizer(const Details& details)
    : details_(details),
      panel_container_(NULL),
      panel_layout_manager_(NULL),
      did_move_or_resize_(false),
      was_attached_(details_.window->GetProperty(internal::kPanelAttachedKey)),
      should_attach_(was_attached_) {
  DCHECK(details_.is_resizable);
  ash::internal::RootWindowController* root_window_controller =
      GetRootWindowController(details.window->GetRootWindow());
  panel_container_ = root_window_controller->GetContainer(
      internal::kShellWindowId_PanelContainer);
  if (panel_container_) {
    panel_layout_manager_ = static_cast<internal::PanelLayoutManager*>(
        panel_container_->layout_manager());
  }
  ash::Shell::GetInstance()->cursor_manager()->LockCursor();
}

bool PanelWindowResizer::AttachToLauncher(gfx::Rect* bounds) {
  bool should_attach = false;
  if (panel_layout_manager_) {
    int launcher_top = panel_layout_manager_->launcher()->widget()->
        GetWindowBoundsInScreen().y();
    if (bounds->y() >= (launcher_top - bounds->height() -
                        kPanelSnapToLauncherDistance)) {
      should_attach = true;
      bounds->set_y(launcher_top - bounds->height());
    }
  }
  return should_attach;
}

void PanelWindowResizer::StartedDragging() {
  if (was_attached_)
    panel_layout_manager_->StartDragging(details_.window);
}

void PanelWindowResizer::FinishDragging() {
  if (was_attached_)
    panel_layout_manager_->FinishDragging();
}

}  // namespace aura
