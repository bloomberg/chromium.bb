// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panels/panel_window_resizer.h"

#include "ash/display/display_controller.h"
#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/panels/panel_layout_manager.h"
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

internal::PanelLayoutManager* GetPanelLayoutManager(
    aura::Window* panel_container) {
  DCHECK(panel_container->id() == internal::kShellWindowId_PanelContainer);
  return static_cast<internal::PanelLayoutManager*>(
      panel_container->layout_manager());
}

}  // namespace

PanelWindowResizer::~PanelWindowResizer() {
  if (destroyed_)
    *destroyed_ = true;
}

// static
PanelWindowResizer*
PanelWindowResizer::Create(WindowResizer* next_window_resizer,
                           aura::Window* window,
                           const gfx::Point& location,
                           int window_component) {
  Details details(window, location, window_component);
  return details.is_resizable ?
      new PanelWindowResizer(next_window_resizer, details) : NULL;
}

void PanelWindowResizer::Drag(const gfx::Point& location, int event_flags) {
  bool destroyed = false;
  if (!did_move_or_resize_) {
    did_move_or_resize_ = true;
    StartedDragging();
  }
  gfx::Point location_in_screen = location;
  wm::ConvertPointToScreen(GetTarget()->parent(), &location_in_screen);

  // Check if the destination has changed displays.
  gfx::Screen* screen = Shell::GetScreen();
  const gfx::Display dst_display =
      screen->GetDisplayNearestPoint(location_in_screen);
  if (dst_display.id() !=
      screen->GetDisplayNearestWindow(panel_container_->GetRootWindow()).id()) {
    // The panel is being dragged to a new display. If the previous container is
    // the current parent of the panel it will be informed of the end of drag
    // when the panel is reparented, otherwise let the previous container know
    // the drag is complete. If we told the panel's parent that the drag was
    // complete it would begin positioning the panel.
    if (GetTarget()->parent() != panel_container_)
      GetPanelLayoutManager(panel_container_)->FinishDragging();
    aura::RootWindow* dst_root = Shell::GetInstance()->display_controller()->
        GetRootWindowForDisplayId(dst_display.id());
    panel_container_ = Shell::GetContainer(
        dst_root, internal::kShellWindowId_PanelContainer);

    // The panel's parent already knows that the drag is in progress for this
    // panel.
    if (panel_container_ && GetTarget()->parent() != panel_container_)
      GetPanelLayoutManager(panel_container_)->StartDragging(GetTarget());
  }
  gfx::Point offset;
  gfx::Rect bounds(CalculateBoundsForDrag(details_, location));
  should_attach_ = AttachToLauncher(bounds, &offset);
  gfx::Point modified_location(location.x() + offset.x(),
                               location.y() + offset.y());
  destroyed_ = &destroyed;
  next_window_resizer_->Drag(modified_location, event_flags);

  // TODO(flackr): Refactor the way WindowResizer calls into other window
  // resizers to avoid the awkward pattern here for checking if
  // next_window_resizer_ destroys the resizer object.
  if (destroyed)
    return;
  destroyed_ = NULL;
  if (should_attach_ &&
      !(details_.bounds_change & WindowResizer::kBoundsChange_Resizes)) {
    UpdateLauncherPosition();
  }
}

void PanelWindowResizer::CompleteDrag(int event_flags) {
  // The root window can change when dragging into a different screen.
  next_window_resizer_->CompleteDrag(event_flags);
  FinishDragging();
}

void PanelWindowResizer::RevertDrag() {
  next_window_resizer_->RevertDrag();
  should_attach_ = was_attached_;
  FinishDragging();
}

aura::Window* PanelWindowResizer::GetTarget() {
  return next_window_resizer_->GetTarget();
}

PanelWindowResizer::PanelWindowResizer(WindowResizer* next_window_resizer,
                                       const Details& details)
    : details_(details),
      next_window_resizer_(next_window_resizer),
      panel_container_(NULL),
      did_move_or_resize_(false),
      was_attached_(GetTarget()->GetProperty(internal::kPanelAttachedKey)),
      should_attach_(was_attached_),
      destroyed_(NULL) {
  DCHECK(details_.is_resizable);
  panel_container_ = Shell::GetContainer(
      details.window->GetRootWindow(),
      internal::kShellWindowId_PanelContainer);
}

bool PanelWindowResizer::AttachToLauncher(const gfx::Rect& bounds,
                                          gfx::Point* offset) {
  bool should_attach = false;
  if (panel_container_) {
    internal::PanelLayoutManager* panel_layout_manager =
        GetPanelLayoutManager(panel_container_);
    gfx::Rect launcher_bounds = ScreenAsh::ConvertRectFromScreen(
        GetTarget()->parent(),
        panel_layout_manager->launcher()->
        shelf_widget()->GetWindowBoundsInScreen());
    switch (panel_layout_manager->launcher()->alignment()) {
      case SHELF_ALIGNMENT_BOTTOM:
        if (bounds.bottom() >= (launcher_bounds.y() -
                                kPanelSnapToLauncherDistance)) {
          should_attach = true;
          offset->set_y(launcher_bounds.y() - bounds.height() - bounds.y());
        }
        break;
      case SHELF_ALIGNMENT_LEFT:
        if (bounds.x() <= (launcher_bounds.right() +
                           kPanelSnapToLauncherDistance)) {
          should_attach = true;
          offset->set_x(launcher_bounds.right() - bounds.x());
        }
        break;
      case SHELF_ALIGNMENT_RIGHT:
        if (bounds.right() >= (launcher_bounds.x() -
                               kPanelSnapToLauncherDistance)) {
          should_attach = true;
          offset->set_x(launcher_bounds.x() - bounds.width() - bounds.x());
        }
        break;
      case SHELF_ALIGNMENT_TOP:
        if (bounds.y() <= (launcher_bounds.bottom() +
                           kPanelSnapToLauncherDistance)) {
          should_attach = true;
          offset->set_y(launcher_bounds.bottom() - bounds.y());
        }
        break;
    }
  }
  return should_attach;
}

void PanelWindowResizer::StartedDragging() {
  // Tell the panel layout manager that we are dragging this panel before
  // attaching it so that it does not get repositioned.
  if (panel_container_)
    GetPanelLayoutManager(panel_container_)->StartDragging(GetTarget());
  if (!was_attached_) {
    // Attach the panel while dragging placing it in front of other panels.
    GetTarget()->SetProperty(internal::kContinueDragAfterReparent, true);
    GetTarget()->SetProperty(internal::kPanelAttachedKey, true);
    GetTarget()->SetDefaultParentByRootWindow(
        GetTarget()->GetRootWindow(),
        GetTarget()->GetBoundsInScreen());
  }
}

void PanelWindowResizer::FinishDragging() {
  if (!did_move_or_resize_)
    return;
  if (GetTarget()->GetProperty(internal::kPanelAttachedKey) !=
      should_attach_) {
    GetTarget()->SetProperty(internal::kPanelAttachedKey, should_attach_);
    GetTarget()->SetDefaultParentByRootWindow(
        GetTarget()->GetRootWindow(),
        GetTarget()->GetBoundsInScreen());
  }
  if (panel_container_)
    GetPanelLayoutManager(panel_container_)->FinishDragging();
}

void PanelWindowResizer::UpdateLauncherPosition() {
  if (panel_container_) {
    GetPanelLayoutManager(panel_container_)->launcher()->
        UpdateIconPositionForWindow(GetTarget());
  }
}

}  // namespace aura
