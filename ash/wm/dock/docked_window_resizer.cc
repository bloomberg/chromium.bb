// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/dock/docked_window_resizer.h"

#include "ash/wm/common/dock/docked_window_layout_manager.h"
#include "ash/wm/common/window_parenting_utils.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/common/wm_event.h"
#include "ash/wm/common/wm_root_window_controller.h"
#include "ash/wm/common/wm_shell_window_ids.h"
#include "ash/wm/common/wm_window.h"
#include "ash/wm/workspace/magnetism_matcher.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"

namespace ash {
namespace {

DockedWindowLayoutManager* GetDockedLayoutManagerAtPoint(
    const gfx::Point& point) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point);
  if (!display.bounds().Contains(point))
    return nullptr;

  return DockedWindowLayoutManager::Get(
      wm::WmRootWindowController::GetWithDisplayId(display.id())->GetWindow());
}

}  // namespace

DockedWindowResizer::~DockedWindowResizer() {
}

// static
DockedWindowResizer*
DockedWindowResizer::Create(WindowResizer* next_window_resizer,
                            wm::WindowState* window_state) {
  return new DockedWindowResizer(next_window_resizer, window_state);
}

void DockedWindowResizer::Drag(const gfx::Point& location, int event_flags) {
  last_location_ = GetTarget()->GetParent()->ConvertPointToScreen(location);
  base::WeakPtr<DockedWindowResizer> resizer(weak_ptr_factory_.GetWeakPtr());

  if (!did_move_or_resize_) {
    did_move_or_resize_ = true;
    StartedDragging(resizer);
  }
  if (!resizer)
    return;

  gfx::Point offset;
  gfx::Rect bounds(CalculateBoundsForDrag(location));
  MaybeSnapToEdge(bounds, &offset);
  gfx::Point modified_location(location);
  modified_location += offset.OffsetFromOrigin();

  next_window_resizer_->Drag(modified_location, event_flags);
  if (!resizer)
    return;

  DockedWindowLayoutManager* new_dock_layout =
      GetDockedLayoutManagerAtPoint(last_location_);
  if (new_dock_layout && new_dock_layout != dock_layout_) {
    // The window is being dragged to a new display. If the previous
    // container is the current parent of the window it will be informed of
    // the end of drag when the window is reparented, otherwise let the
    // previous container know the drag is complete. If we told the
    // window's parent that the drag was complete it would begin
    // positioning the window.
    if (is_docked_ && dock_layout_->is_dragged_window_docked())
      dock_layout_->UndockDraggedWindow();
    if (dock_layout_ != initial_dock_layout_)
      dock_layout_->FinishDragging(
          DOCKED_ACTION_NONE,
          details().source == aura::client::WINDOW_MOVE_SOURCE_MOUSE ?
              DOCKED_ACTION_SOURCE_MOUSE : DOCKED_ACTION_SOURCE_TOUCH);
    is_docked_ = false;
    dock_layout_ = new_dock_layout;
    // The window's initial layout manager already knows that the drag is
    // in progress for this window.
    if (new_dock_layout != initial_dock_layout_)
      new_dock_layout->StartDragging(GetTarget());
  }
  // Window could get docked by the WorkspaceWindowResizer, update the state.
  is_docked_ = dock_layout_->is_dragged_window_docked();
  // Whenever a window is dragged out of the dock it will be auto-sized
  // in the dock if it gets docked again.
  if (!is_docked_)
    was_bounds_changed_by_user_ = false;
}

void DockedWindowResizer::CompleteDrag() {
  // The root window can change when dragging into a different screen.
  next_window_resizer_->CompleteDrag();
  FinishedDragging(aura::client::MOVE_SUCCESSFUL);
}

void DockedWindowResizer::RevertDrag() {
  next_window_resizer_->RevertDrag();
  // Restore docked state to what it was before the drag if necessary.
  if (is_docked_ != was_docked_) {
    is_docked_ = was_docked_;
    if (is_docked_)
      dock_layout_->DockDraggedWindow(GetTarget());
    else
      dock_layout_->UndockDraggedWindow();
  }
  FinishedDragging(aura::client::MOVE_CANCELED);
}

DockedWindowResizer::DockedWindowResizer(WindowResizer* next_window_resizer,
                                         wm::WindowState* window_state)
    : WindowResizer(window_state),
      next_window_resizer_(next_window_resizer),
      dock_layout_(NULL),
      initial_dock_layout_(NULL),
      did_move_or_resize_(false),
      was_docked_(false),
      is_docked_(false),
      was_bounds_changed_by_user_(window_state->bounds_changed_by_user()),
      weak_ptr_factory_(this) {
  DCHECK(details().is_resizable);
  dock_layout_ = DockedWindowLayoutManager::Get(GetTarget()->GetRootWindow());
  initial_dock_layout_ = dock_layout_;
  was_docked_ = GetTarget()->GetParent() == dock_layout_->dock_container();
  is_docked_ = was_docked_;
}

void DockedWindowResizer::MaybeSnapToEdge(const gfx::Rect& bounds,
                                          gfx::Point* offset) {
  // Windows only snap magnetically when they were previously docked.
  if (!was_docked_)
    return;
  DockedAlignment dock_alignment = dock_layout_->CalculateAlignment();
  gfx::Rect dock_bounds = GetTarget()->GetParent()->ConvertRectFromScreen(
      dock_layout_->dock_container()->GetBoundsInScreen());

  // Short-range magnetism when retaining docked state. Same constant as in
  // MagnetismMatcher is used for consistency.
  const int kSnapToDockDistance = MagnetismMatcher::kMagneticDistance;

  if (dock_alignment == DOCKED_ALIGNMENT_LEFT ||
      dock_alignment == DOCKED_ALIGNMENT_NONE) {
    const int distance = bounds.x() - dock_bounds.x();
    if (distance < kSnapToDockDistance && distance > 0) {
      offset->set_x(-distance);
      return;
    }
  }
  if (dock_alignment == DOCKED_ALIGNMENT_RIGHT ||
      dock_alignment == DOCKED_ALIGNMENT_NONE) {
    const int distance = dock_bounds.right() - bounds.right();
    if (distance < kSnapToDockDistance && distance > 0)
      offset->set_x(distance);
  }
}

void DockedWindowResizer::StartedDragging(
    base::WeakPtr<DockedWindowResizer>& resizer) {
  // During resizing the window width is preserved by DockedwindowLayoutManager.
  if (is_docked_ &&
      (details().bounds_change & WindowResizer::kBoundsChange_Resizes)) {
    window_state_->set_bounds_changed_by_user(true);
  }

  // Tell the dock layout manager that we are dragging this window.
  // At this point we are not yet animating the window as it may not be
  // inside the docked area.
  dock_layout_->StartDragging(GetTarget());
  if (!resizer)
    return;
  // Reparent workspace windows during the drag to elevate them above workspace.
  // Other windows for which the DockedWindowResizer is instantiated include
  // panels and windows that are already docked. Those do not need reparenting.
  if (GetTarget()->GetType() != ui::wm::WINDOW_TYPE_PANEL &&
      GetTarget()->GetParent()->GetShellWindowId() ==
          kShellWindowId_DefaultContainer) {
    // Reparent the window into the docked windows container in order to get it
    // on top of other docked windows.
    wm::WmWindow* docked_container =
        GetTarget()->GetRootWindow()->GetChildByShellWindowId(
            kShellWindowId_DockedContainer);
    ReparentChildWithTransientChildren(GetTarget(), GetTarget()->GetParent(),
                                       docked_container);
    if (!resizer)
      return;
  }
  if (is_docked_)
    dock_layout_->DockDraggedWindow(GetTarget());
}

void DockedWindowResizer::FinishedDragging(
    aura::client::WindowMoveResult move_result) {
  if (!did_move_or_resize_)
    return;
  did_move_or_resize_ = false;
  wm::WmWindow* window = GetTarget();
  const bool is_attached_panel =
      window->GetType() == ui::wm::WINDOW_TYPE_PANEL &&
      window_state_->panel_attached();
  const bool is_resized =
      (details().bounds_change & WindowResizer::kBoundsChange_Resizes) != 0;

  // Undock the window if it is not in the normal, docked or minimized state
  // type. This happens if a user snaps or maximizes a window using a
  // keyboard shortcut while it is being dragged.
  if (!window_state_->IsMinimized() && !window_state_->IsDocked() &&
      !window_state_->IsNormalStateType())
    is_docked_ = false;

  // When drag is completed the dragged docked window is resized to the bounds
  // calculated by the layout manager that conform to other docked windows.
  if (!is_attached_panel && is_docked_ && !is_resized) {
    gfx::Rect bounds = window->GetParent()->ConvertRectFromScreen(
        dock_layout_->dragged_bounds());
    if (!bounds.IsEmpty() && bounds.width() != window->GetBounds().width()) {
      window->SetBounds(bounds);
    }
  }
  // If a window has restore bounds, update the restore origin but not the size.
  // The size gets restored when a window is undocked.
  if (is_resized && is_docked_ && window_state_->HasRestoreBounds()) {
    gfx::Rect restore_bounds = window->GetBoundsInScreen();
    restore_bounds.set_size(window_state_->GetRestoreBoundsInScreen().size());
    window_state_->SetRestoreBoundsInScreen(restore_bounds);
  }

  // Check if the window needs to be docked or returned to workspace.
  DockedAction action = MaybeReparentWindowOnDragCompletion(is_resized,
                                                            is_attached_panel);
  dock_layout_->FinishDragging(
      move_result == aura::client::MOVE_CANCELED ? DOCKED_ACTION_NONE : action,
      details().source == aura::client::WINDOW_MOVE_SOURCE_MOUSE ?
          DOCKED_ACTION_SOURCE_MOUSE : DOCKED_ACTION_SOURCE_TOUCH);

  // If we started the drag in one root window and moved into another root
  // but then canceled the drag we may need to inform the original layout
  // manager that the drag is finished.
  if (initial_dock_layout_ != dock_layout_)
    initial_dock_layout_->FinishDragging(
        DOCKED_ACTION_NONE,
        details().source == aura::client::WINDOW_MOVE_SOURCE_MOUSE ?
            DOCKED_ACTION_SOURCE_MOUSE : DOCKED_ACTION_SOURCE_TOUCH);
  is_docked_ = false;
}

DockedAction DockedWindowResizer::MaybeReparentWindowOnDragCompletion(
    bool is_resized, bool is_attached_panel) {
  wm::WmWindow* window = GetTarget();

  // Check if the window needs to be docked or returned to workspace.
  DockedAction action = DOCKED_ACTION_NONE;
  wm::WmWindow* dock_container =
      window->GetRootWindow()->GetChildByShellWindowId(
          kShellWindowId_DockedContainer);
  if ((is_resized || !is_attached_panel) &&
      is_docked_ != (window->GetParent() == dock_container)) {
    if (is_docked_) {
      wm::ReparentChildWithTransientChildren(window, window->GetParent(),
                                             dock_container);
      action = DOCKED_ACTION_DOCK;
    } else if (window->GetParent()->GetShellWindowId() ==
               kShellWindowId_DockedContainer) {
      // Reparent the window back to workspace.
      // We need to be careful to give ParentWindowWithContext a location in
      // the right root window (matching the logic in DragWindowResizer) based
      // on which root window a mouse pointer is in. We want to undock into the
      // right screen near the edge of a multiscreen setup (based on where the
      // mouse is).
      gfx::Rect near_last_location(last_location_, gfx::Size());
      // Reparenting will cause Relayout and possible dock shrinking.
      wm::WmWindow* previous_parent = window->GetParent();
      window->SetParentUsingContext(window, near_last_location);
      if (window->GetParent() != previous_parent) {
        wm::ReparentTransientChildrenOfChild(window, previous_parent,
                                             window->GetParent());
      }
      action = was_docked_ ? DOCKED_ACTION_UNDOCK : DOCKED_ACTION_NONE;
    }
  } else {
    // |action| is recorded in UMA and used to maintain |window_state_|.
    if (is_resized && is_docked_ && was_docked_)
      action = DOCKED_ACTION_RESIZE;
    else if (is_docked_ && was_docked_)
      action = DOCKED_ACTION_REORDER;
    else if (is_docked_ && !was_docked_)
      action = DOCKED_ACTION_DOCK;
    else if (!is_docked_ && was_docked_)
      action = DOCKED_ACTION_UNDOCK;
    else
      action = DOCKED_ACTION_NONE;
  }
  // When a window is newly docked it is auto-sized by docked layout adjusting
  // to other windows. If it is just dragged (but not resized) while being
  // docked it is auto-sized unless it has been resized while being docked
  // before.
  if (is_docked_) {
    window->GetWindowState()->set_bounds_changed_by_user(
        was_docked_ && (is_resized || was_bounds_changed_by_user_));
  }

  if (action == DOCKED_ACTION_DOCK) {
    const wm::WMEvent event(wm::WM_EVENT_DOCK);
    window_state_->OnWMEvent(&event);
  } else if (window->GetWindowState()->IsDocked() &&
             action == DOCKED_ACTION_UNDOCK) {
    const wm::WMEvent event(wm::WM_EVENT_NORMAL);
    window_state_->OnWMEvent(&event);
  }

  return action;
}

}  // namespace ash
