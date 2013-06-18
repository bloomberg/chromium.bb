// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/dock/docked_window_resizer.h"

#include "ash/ash_switches.h"
#include "ash/display/display_controller.h"
#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/dock/docked_window_layout_manager.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/workspace_controller.h"
#include "base/command_line.h"
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

// How far we have to drag to undock a window.
const int kSnapToDockDistance = 32;

}  // namespace

DockedWindowResizer::~DockedWindowResizer() {
}

// static
DockedWindowResizer*
DockedWindowResizer::Create(WindowResizer* next_window_resizer,
                            aura::Window* window,
                            const gfx::Point& location,
                            int window_component,
                            aura::client::WindowMoveSource source) {
  Details details(window, location, window_component, source);
  return details.is_resizable ?
      new DockedWindowResizer(next_window_resizer, details) : NULL;
}

void DockedWindowResizer::Drag(const gfx::Point& location, int event_flags) {
  last_location_ = location;
  wm::ConvertPointToScreen(GetTarget()->parent(), &last_location_);
  if (!did_move_or_resize_) {
    did_move_or_resize_ = true;
    StartedDragging();
  }
  gfx::Point offset;
  gfx::Rect bounds(CalculateBoundsForDrag(details_, location));
  MaybeSnapToSide(bounds, &offset);
  gfx::Point modified_location(location.x() + offset.x(),
                               location.y() + offset.y());
  next_window_resizer_->Drag(modified_location, event_flags);
}

void DockedWindowResizer::CompleteDrag(int event_flags) {
  // The root window can change when dragging into a different screen.
  next_window_resizer_->CompleteDrag(event_flags);
  FinishDragging();
}

void DockedWindowResizer::RevertDrag() {
  next_window_resizer_->RevertDrag();
  FinishDragging();
}

aura::Window* DockedWindowResizer::GetTarget() {
  return next_window_resizer_->GetTarget();
}

const gfx::Point& DockedWindowResizer::GetInitialLocation() const {
  return details_.initial_location_in_parent;
}

DockedWindowResizer::DockedWindowResizer(WindowResizer* next_window_resizer,
                                         const Details& details)
    : details_(details),
      next_window_resizer_(next_window_resizer),
      dock_layout_(NULL),
      did_move_or_resize_(false) {
  DCHECK(details_.is_resizable);
  aura::Window* dock_container = Shell::GetContainer(
      details.window->GetRootWindow(),
      internal::kShellWindowId_DockedContainer);
  DCHECK(dock_container->id() == internal::kShellWindowId_DockedContainer);
  dock_layout_ = static_cast<internal::DockedWindowLayoutManager*>(
      dock_container->layout_manager());
}

void DockedWindowResizer::MaybeSnapToSide(const gfx::Rect& bounds,
                                          gfx::Point* offset) {
  aura::Window* dock_container = Shell::GetContainer(
      wm::GetRootWindowAt(last_location_),
      internal::kShellWindowId_DockedContainer);
  DCHECK(dock_container->id() == internal::kShellWindowId_DockedContainer);
  internal::DockedWindowLayoutManager* dock_layout =
      static_cast<internal::DockedWindowLayoutManager*>(
          dock_container->layout_manager());

  internal::DockedAlignment dock_alignment = dock_layout->alignment();
  if (dock_alignment == internal::DOCKED_ALIGNMENT_NONE) {
    if (GetTarget() != dock_layout_->dragged_window())
      return;
    dock_alignment = internal::DOCKED_ALIGNMENT_ANY;
  }

  gfx::Rect dock_bounds = ScreenAsh::ConvertRectFromScreen(
      GetTarget()->parent(), dock_container->GetBoundsInScreen());

  switch (dock_alignment) {
    case internal::DOCKED_ALIGNMENT_LEFT:
      if (abs(bounds.x() - dock_bounds.x()) < kSnapToDockDistance)
        offset->set_x(dock_bounds.x() - bounds.x());
      break;
    case internal::DOCKED_ALIGNMENT_RIGHT:
      if (abs(bounds.right() - dock_bounds.right()) < kSnapToDockDistance)
        offset->set_x(dock_bounds.right() - bounds.right());
      break;
    case internal::DOCKED_ALIGNMENT_ANY:
      if (abs(bounds.x() - dock_bounds.x()) < kSnapToDockDistance)
        offset->set_x(dock_bounds.x() - bounds.x());
      if (abs(bounds.right() - dock_bounds.right()) < kSnapToDockDistance)
        offset->set_x(dock_bounds.right() - bounds.right());
      break;
    case internal::DOCKED_ALIGNMENT_NONE:
      NOTREACHED() << "Nothing to do when no windows are docked";
      break;
  }
}

void DockedWindowResizer::StartedDragging() {
  // Tell the dock layout manager that we are dragging this window.
  dock_layout_->StartDragging(GetTarget());
}

void DockedWindowResizer::FinishDragging() {
  if (!did_move_or_resize_)
    return;

  aura::Window* window = GetTarget();
  bool should_dock = false;
  if ((details_.bounds_change & WindowResizer::kBoundsChange_Repositions) &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableDockedWindows)) {
    should_dock = internal::DockedWindowLayoutManager::ShouldWindowDock(
        window, last_location_);
  }

  // Check if desired docked state is not same as current.
  // If not same dock or undock accordingly.
  if (should_dock !=
      (window->parent()->id() == internal::kShellWindowId_DockedContainer)) {
    if (should_dock) {
      aura::Window* dock_container = Shell::GetContainer(
          window->GetRootWindow(),
          internal::kShellWindowId_DockedContainer);
      dock_container->AddChild(window);
    } else {
      // Reparent the window back to workspace.
      // We need to be careful to give SetDefaultParentByRootWindow location in
      // the right root window (matching the logic in DragWindowResizer) based
      // on which root window a mouse pointer is in. We want to undock into the
      // right screen near the edge of a multiscreen setup (based on where the
      // mouse is).
      gfx::Rect near_last_location(last_location_, gfx::Size());
      // Reparenting will cause Relayout and possible dock shrinking.
      window->SetDefaultParentByRootWindow(window->GetRootWindow(),
                                           near_last_location);
      // A maximized workspace may be active so we may need to switch
      // to a parent workspace of the window being dragged out.
      internal::WorkspaceController* workspace_controller =
          GetRootWindowController(
              window->GetRootWindow())->workspace_controller();
      workspace_controller->SetActiveWorkspaceByWindow(window);
    }
  }
  dock_layout_->FinishDragging();
}

}  // namespace aura
