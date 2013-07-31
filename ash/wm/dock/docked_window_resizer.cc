// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/dock/docked_window_resizer.h"

#include "ash/ash_switches.h"
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

// Distance in pixels that the cursor must move past an edge for a window
// to move beyond that edge.
const int kStickyDistance = 64;

// How far in pixels we have to drag to undock a window.
const int kSnapToDockDistance = 32;

}  // namespace

DockedWindowResizer::~DockedWindowResizer() {
  if (destroyed_)
    *destroyed_ = true;
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
  bool destroyed = false;
  if (!did_move_or_resize_) {
    did_move_or_resize_ = true;
    StartedDragging();
  }
  gfx::Point offset;
  gfx::Rect bounds(CalculateBoundsForDrag(details_, location));
  bool set_tracked_by_workspace = MaybeSnapToEdge(bounds, &offset);

  // Temporarily clear kWindowTrackedByWorkspaceKey for windows that are snapped
  // to screen edges e.g. when they are docked. This prevents the windows from
  // getting snapped to other nearby windows during the drag.
  bool tracked_by_workspace = GetTrackedByWorkspace(GetTarget());
  if (set_tracked_by_workspace)
    SetTrackedByWorkspace(GetTarget(), false);
  gfx::Point modified_location(location.x() + offset.x(),
                               location.y() + offset.y());
  destroyed_ = &destroyed;
  next_window_resizer_->Drag(modified_location, event_flags);

  // TODO(varkha): Refactor the way WindowResizer calls into other window
  // resizers to avoid the awkward pattern here for checking if
  // next_window_resizer_ destroys the resizer object.
  if (destroyed)
    return;
  destroyed_ = NULL;
  if (set_tracked_by_workspace)
    SetTrackedByWorkspace(GetTarget(), tracked_by_workspace);
}

void DockedWindowResizer::CompleteDrag(int event_flags) {
  // Temporarily clear kWindowTrackedByWorkspaceKey for panels so that they
  // don't get forced into the workspace that may be shrunken because of docked
  // windows.
  bool tracked_by_workspace = GetTrackedByWorkspace(GetTarget());
  bool set_tracked_by_workspace =
      (was_docked_ && GetTarget()->type() == aura::client::WINDOW_TYPE_PANEL);
  if (set_tracked_by_workspace)
    SetTrackedByWorkspace(GetTarget(), false);
  // The root window can change when dragging into a different screen.
  next_window_resizer_->CompleteDrag(event_flags);
  if (set_tracked_by_workspace)
    SetTrackedByWorkspace(GetTarget(), tracked_by_workspace);
  FinishDragging();
}

void DockedWindowResizer::RevertDrag() {
  // Temporarily clear kWindowTrackedByWorkspaceKey for panels so that they
  // don't get forced into the workspace that may be shrunken because of docked
  // windows.
  bool tracked_by_workspace = GetTrackedByWorkspace(GetTarget());
  bool set_tracked_by_workspace =
      (was_docked_ && GetTarget()->type() == aura::client::WINDOW_TYPE_PANEL);
  if (set_tracked_by_workspace)
    SetTrackedByWorkspace(GetTarget(), false);
  next_window_resizer_->RevertDrag();
  if (set_tracked_by_workspace)
    SetTrackedByWorkspace(GetTarget(), tracked_by_workspace);
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
      did_move_or_resize_(false),
      was_docked_(false),
      destroyed_(NULL) {
  DCHECK(details_.is_resizable);
  aura::Window* dock_container = Shell::GetContainer(
      details.window->GetRootWindow(),
      internal::kShellWindowId_DockedContainer);
  DCHECK(dock_container->id() == internal::kShellWindowId_DockedContainer);
  dock_layout_ = static_cast<internal::DockedWindowLayoutManager*>(
      dock_container->layout_manager());
  was_docked_ = details.window->parent() == dock_container;
}

bool DockedWindowResizer::MaybeSnapToEdge(const gfx::Rect& bounds,
                                          gfx::Point* offset) {
  aura::Window* dock_container = Shell::GetContainer(
      wm::GetRootWindowAt(last_location_),
      internal::kShellWindowId_DockedContainer);
  DCHECK(dock_container->id() == internal::kShellWindowId_DockedContainer);
  internal::DockedWindowLayoutManager* dock_layout =
      static_cast<internal::DockedWindowLayoutManager*>(
          dock_container->layout_manager());
  internal::DockedAlignment dock_alignment = dock_layout->CalculateAlignment();
  gfx::Rect dock_bounds = ScreenAsh::ConvertRectFromScreen(
      GetTarget()->parent(), dock_container->GetBoundsInScreen());
  // Windows only snap magnetically when they are close to the edge of the
  // screen and when the cursor is over other docked windows.
  // When a window being dragged is the last window that was previously
  // docked it is still allowed to magnetically snap to either side.
  bool can_snap = was_docked_ ||
      internal::DockedWindowLayoutManager::ShouldWindowDock(GetTarget(),
                                                            last_location_);
  if (!can_snap)
    return false;

  if (dock_alignment == internal::DOCKED_ALIGNMENT_LEFT ||
      (dock_alignment == internal::DOCKED_ALIGNMENT_NONE && was_docked_)) {
    const int distance = bounds.x() - dock_bounds.x();
    if (distance < kSnapToDockDistance && distance > -kStickyDistance) {
      offset->set_x(-distance);
      return true;
    }
  }
  if (dock_alignment == internal::DOCKED_ALIGNMENT_RIGHT ||
      (dock_alignment == internal::DOCKED_ALIGNMENT_NONE && was_docked_)) {
    const int distance = dock_bounds.right() - bounds.right();
    if (distance < kSnapToDockDistance && distance > -kStickyDistance) {
      offset->set_x(distance);
      return true;
    }
  }
  return false;
}

void DockedWindowResizer::StartedDragging() {
  // Tell the dock layout manager that we are dragging this window.
  dock_layout_->StartDragging(GetTarget());
}

void DockedWindowResizer::FinishDragging() {
  if (!did_move_or_resize_)
    return;

  aura::Window* window = GetTarget();
  bool should_dock = was_docked_;
  if ((details_.bounds_change & WindowResizer::kBoundsChange_Repositions) &&
      !(details_.bounds_change & WindowResizer::kBoundsChange_Resizes) &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableDockedWindows)) {
    const bool attached_panel =
        window->type() == aura::client::WINDOW_TYPE_PANEL &&
        window->GetProperty(internal::kPanelAttachedKey);
    should_dock = !attached_panel &&
        internal::DockedWindowLayoutManager::ShouldWindowDock(window,
                                                              last_location_);
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
    }
  }
  dock_layout_->FinishDragging();
}

}  // namespace aura
