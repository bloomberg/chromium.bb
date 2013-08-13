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
#include "ash/wm/workspace/magnetism_matcher.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
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
namespace internal {

namespace {

DockedWindowLayoutManager* GetDockedLayoutManagerAtPoint(
    const gfx::Point& point) {
  aura::Window* dock_container = Shell::GetContainer(
      wm::GetRootWindowAt(point),
      kShellWindowId_DockedContainer);
  return static_cast<DockedWindowLayoutManager*>(
      dock_container->layout_manager());
}

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

  DockedWindowLayoutManager* new_dock_layout =
      GetDockedLayoutManagerAtPoint(last_location_);
  if (new_dock_layout != dock_layout_) {
    // The window is being dragged to a new display. If the previous
    // container is the current parent of the window it will be informed of
    // the end of drag when the window is reparented, otherwise let the
    // previous container know the drag is complete. If we told the
    // window's parent that the drag was complete it would begin
    // positioning the window.
    if (is_docked_)
      dock_layout_->UndockDraggedWindow();
    if (dock_layout_ != initial_dock_layout_)
      dock_layout_->FinishDragging();
    is_docked_ = false;
    dock_layout_ = new_dock_layout;
    // The window's initial layout manager already knows that the drag is
    // in progress for this window.
    if (new_dock_layout != initial_dock_layout_)
      new_dock_layout->StartDragging(GetTarget());
  }

  // Show snapping animation when a window touches a screen edge or when
  // it is about to get docked.
  DockedAlignment new_docked_alignment = GetDraggedWindowAlignment();
  if (new_docked_alignment != DOCKED_ALIGNMENT_NONE) {
    if (!is_docked_) {
      dock_layout_->DockDraggedWindow(GetTarget());
      is_docked_ = true;
    }
    UpdateSnapPhantomWindow();
  } else {
    if (is_docked_) {
      dock_layout_->UndockDraggedWindow();
      is_docked_ = false;
    }
    // Clear phantom window when a window gets undocked.
    snap_phantom_window_controller_.reset();
  }
}

void DockedWindowResizer::CompleteDrag(int event_flags) {
  snap_phantom_window_controller_.reset();

  // Temporarily clear kWindowTrackedByWorkspaceKey for panels so that they
  // don't get forced into the workspace that may be shrunken because of docked
  // windows.
  bool tracked_by_workspace = GetTrackedByWorkspace(GetTarget());
  bool set_tracked_by_workspace = was_docked_;
  if (set_tracked_by_workspace)
    SetTrackedByWorkspace(GetTarget(), false);
  // The root window can change when dragging into a different screen.
  next_window_resizer_->CompleteDrag(event_flags);
  FinishedDragging();
  if (set_tracked_by_workspace)
    SetTrackedByWorkspace(GetTarget(), tracked_by_workspace);
}

void DockedWindowResizer::RevertDrag() {
  snap_phantom_window_controller_.reset();

  // Temporarily clear kWindowTrackedByWorkspaceKey for panels so that they
  // don't get forced into the workspace that may be shrunken because of docked
  // windows.
  bool tracked_by_workspace = GetTrackedByWorkspace(GetTarget());
  bool set_tracked_by_workspace = was_docked_;
  if (set_tracked_by_workspace)
    SetTrackedByWorkspace(GetTarget(), false);
  next_window_resizer_->RevertDrag();
  FinishedDragging();
  if (set_tracked_by_workspace)
    SetTrackedByWorkspace(GetTarget(), tracked_by_workspace);
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
      initial_dock_layout_(NULL),
      did_move_or_resize_(false),
      was_docked_(false),
      is_docked_(false),
      destroyed_(NULL) {
  DCHECK(details_.is_resizable);
  aura::Window* dock_container = Shell::GetContainer(
      details.window->GetRootWindow(),
      kShellWindowId_DockedContainer);
  dock_layout_ = static_cast<DockedWindowLayoutManager*>(
      dock_container->layout_manager());
  initial_dock_layout_ = dock_layout_;
  was_docked_ = details.window->parent() == dock_container;
  is_docked_ = was_docked_;
}

DockedAlignment DockedWindowResizer::GetDraggedWindowAlignment() {
  aura::Window* window = GetTarget();
  DockedWindowLayoutManager* layout_manager =
      GetDockedLayoutManagerAtPoint(last_location_);
  const DockedAlignment alignment = layout_manager->CalculateAlignment();
  const gfx::Rect& bounds(window->GetBoundsInScreen());

  // Check if the window is touching the edge - it may need to get docked.
  if (alignment == DOCKED_ALIGNMENT_NONE)
    return layout_manager->GetAlignmentOfWindow(window);

  // Both bounds and pointer location are checked because some drags involve
  // stickiness at the workspace-to-dock boundary and so the |location| may be
  // outside of the |bounds|.
  // It is also possible that all the docked windows are minimized or hidden
  // in which case the dragged window needs to be exactly touching the same
  // edge that those docked windows were aligned before they got minimized.
  // TODO(varkha): Consider eliminating sticky behavior on that boundary when
  // a pointer enters docked area.
  if ((layout_manager->docked_bounds().Intersects(bounds) &&
       layout_manager->docked_bounds().Contains(last_location_)) ||
      alignment == layout_manager->GetAlignmentOfWindow(window)) {
    // A window is being added to other docked windows (on the same side).
    return alignment;
  }
  return DOCKED_ALIGNMENT_NONE;
}

bool DockedWindowResizer::MaybeSnapToEdge(const gfx::Rect& bounds,
                                          gfx::Point* offset) {
  aura::Window* dock_container = Shell::GetContainer(
      wm::GetRootWindowAt(last_location_),
      kShellWindowId_DockedContainer);
  DockedAlignment dock_alignment =
      GetDockedLayoutManagerAtPoint(last_location_)->CalculateAlignment();
  gfx::Rect dock_bounds = ScreenAsh::ConvertRectFromScreen(
      GetTarget()->parent(), dock_container->GetBoundsInScreen());
  // Windows only snap magnetically when they are close to the edge of the
  // screen and when the cursor is over other docked windows.
  // When a window being dragged is the last window that was previously
  // docked it is still allowed to magnetically snap to either side.
  bool can_snap = was_docked_ ||
      (GetDraggedWindowAlignment() != DOCKED_ALIGNMENT_NONE);
  if (!can_snap)
    return false;

  // Distance in pixels that the cursor must move past an edge for a window
  // to move beyond that edge. Same constant as in WorkspaceWindowResizer
  // is used for consistency.
  const int kStickyDistance = WorkspaceWindowResizer::kStickyDistancePixels;

  // Short-range magnetism when retaining docked state. Same constant as in
  // MagnetismMatcher is used for consistency.
  const int kSnapToDockDistance = MagnetismMatcher::kMagneticDistance;

  if (dock_alignment == DOCKED_ALIGNMENT_LEFT ||
      (dock_alignment == DOCKED_ALIGNMENT_NONE && was_docked_)) {
    const int distance = bounds.x() - dock_bounds.x();
    if (distance < (was_docked_ ? kSnapToDockDistance : 0) &&
        distance > -kStickyDistance) {
      offset->set_x(-distance);
      return true;
    }
  }
  if (dock_alignment == DOCKED_ALIGNMENT_RIGHT ||
      (dock_alignment == DOCKED_ALIGNMENT_NONE && was_docked_)) {
    const int distance = dock_bounds.right() - bounds.right();
    if (distance < (was_docked_ ? kSnapToDockDistance : 0) &&
        distance > -kStickyDistance) {
      offset->set_x(distance);
      return true;
    }
  }
  return false;
}

void DockedWindowResizer::StartedDragging() {
  // Tell the dock layout manager that we are dragging this window.
  // At this point we are not yet animating the window as it may not be
  // inside the docked area.
  dock_layout_->StartDragging(GetTarget());
  // Reparent workspace windows during the drag to elevate them above workspace.
  // Other windows for which the DockedWindowResizer is instantiated include
  // panels and windows that are already docked. Those do not need reparenting.
  if (GetTarget()->type() != aura::client::WINDOW_TYPE_PANEL &&
      GetTarget()->parent()->id() == kShellWindowId_DefaultContainer) {
    // The window is going to be reparented - avoid completing the drag.
    GetTarget()->SetProperty(kContinueDragAfterReparent, true);

    // Reparent the window into the docked windows container in order to get it
    // on top of other docked windows.
    aura::Window* docked_container = Shell::GetContainer(
        GetTarget()->GetRootWindow(),
        kShellWindowId_DockedContainer);
    docked_container->AddChild(GetTarget());
  }
  if (is_docked_)
    dock_layout_->DockDraggedWindow(GetTarget());
}

void DockedWindowResizer::FinishedDragging() {
  if (!did_move_or_resize_)
    return;

  aura::Window* window = GetTarget();
  bool should_dock = was_docked_;
  const bool attached_panel =
      window->type() == aura::client::WINDOW_TYPE_PANEL &&
      window->GetProperty(kPanelAttachedKey);
  // If a window was previously docked then keep it docked if it is resized and
  // still aligned at the screen edge.
  if ((was_docked_ ||
       ((details_.bounds_change & WindowResizer::kBoundsChange_Repositions) &&
        !(details_.bounds_change & WindowResizer::kBoundsChange_Resizes)))) {
    should_dock = GetDraggedWindowAlignment() != DOCKED_ALIGNMENT_NONE;
  }

  // Check if the window needs to be docked or returned to workspace.
  aura::Window* dock_container = Shell::GetContainer(
      window->GetRootWindow(),
      kShellWindowId_DockedContainer);
  if (!attached_panel &&
      should_dock != (window->parent() == dock_container)) {
    if (should_dock) {
      dock_container->AddChild(window);
    } else if (window->parent()->id() == kShellWindowId_DockedContainer) {
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

  // If we started the drag in one root window and moved into another root
  // but then canceled the drag we may need to inform the original layout
  // manager that the drag is finished.
  if (initial_dock_layout_ != dock_layout_)
    initial_dock_layout_->FinishDragging();
  is_docked_ = false;
}

void DockedWindowResizer::UpdateSnapPhantomWindow() {
  if (!did_move_or_resize_ || details_.window_component != HTCAPTION)
    return;

  if (!snap_phantom_window_controller_) {
    snap_phantom_window_controller_.reset(
        new PhantomWindowController(GetTarget()));
  }
  snap_phantom_window_controller_->Show(dock_layout_->dragged_bounds());
}

}  // namespace internal
}  // namespace ash
