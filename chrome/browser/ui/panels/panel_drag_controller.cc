// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_drag_controller.h"

#include "base/logging.h"
#include "chrome/browser/ui/panels/detached_panel_collection.h"
#include "chrome/browser/ui/panels/detached_panel_drag_handler.h"
#include "chrome/browser/ui/panels/docked_panel_collection.h"
#include "chrome/browser/ui/panels/docked_panel_drag_handler.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"
#include "chrome/browser/ui/panels/stacked_panel_drag_handler.h"

namespace {

// The minimum distance that the docked panel gets dragged up in order to
// make it free-floating.
const int kDetachDockedPanelThreshold = 100;

// Indicates how close the bottom of the detached panel is to the bottom of
// the docked area such that the detached panel becomes docked.
const int kDockDetachedPanelThreshold = 30;

// The minimum distance and overlap (in pixels) between two panels such that
// they can be stacked/snapped together.
const int kGluePanelsDistanceThreshold = 15;
const int kGluePanelsOverlapThreshold = 10;

int GetHorizontalOverlap(const gfx::Rect& bounds1, const gfx::Rect& bounds2) {
  // Check for no overlap.
  if (bounds1.right() <= bounds2.x() || bounds1.x() >= bounds2.right())
    return 0;

  // Check for complete overlap.
  if (bounds2.x() <= bounds1.x() && bounds1.right() <= bounds2.right())
    return bounds1.width();

  if (bounds1.x() <= bounds2.x() && bounds2.right() <= bounds1.right())
    return bounds2.width();

  // Compute the overlap part.
  return (bounds1.x() < bounds2.x()) ? (bounds1.right() - bounds2.x())
                                     : (bounds2.right() - bounds1.x());
}

int GetVerticalOverlap(const gfx::Rect& bounds1, const gfx::Rect& bounds2) {
  // Check for no overlap.
  if (bounds1.bottom() <= bounds2.y() || bounds1.y() >= bounds2.bottom())
    return 0;

  // Check for complete overlap.
  if (bounds2.y() <= bounds1.y() && bounds1.bottom() <= bounds2.bottom())
    return bounds1.height();

  if (bounds1.y() <= bounds2.y() && bounds2.bottom() <= bounds1.bottom())
    return bounds2.height();

  // Compute the overlap part.
  return (bounds1.y() < bounds2.y()) ? (bounds1.bottom() - bounds2.y())
                                     : (bounds2.bottom() - bounds1.y());
}

// Return the vertical distance between the bottom edge of |top_bounds| and
// the top edge of |bottom_bounds|.
int GetVerticalDistance(const gfx::Rect& top_bounds,
                        const gfx::Rect& bottom_bounds) {
  return abs(bottom_bounds.y() - top_bounds.bottom());
}

// Return the vertical distance between the right edge of |left_bounds| and
// the left edge of |right_bounds|.
int GetHorizontalDistance(const gfx::Rect& left_bounds,
                          const gfx::Rect& right_bounds) {
  return abs(right_bounds.x() - left_bounds.right());
}

void SetPreviewModeForPanelAndBelow(Panel* panel, bool in_preview) {
  StackedPanelCollection* stack = panel->stack();
  if (stack) {
    bool panel_found = false;
    for (StackedPanelCollection::Panels::const_iterator iter =
             stack->panels().begin();
         iter != stack->panels().end(); ++iter) {
      Panel* current_panel = *iter;
      if (!panel_found && current_panel != panel)
        continue;
      panel_found = true;
      if (in_preview != current_panel->in_preview_mode())
        current_panel->SetPreviewMode(in_preview);
    }
  } else {
    panel->SetPreviewMode(in_preview);
  }
}

}  // namespace

// static
int PanelDragController::GetDetachDockedPanelThresholdForTesting() {
  return kDetachDockedPanelThreshold;
}

// static
int PanelDragController::GetDockDetachedPanelThresholdForTesting() {
  return kDockDetachedPanelThreshold;
}

// static
int PanelDragController::GetGluePanelDistanceThresholdForTesting() {
  return kGluePanelsDistanceThreshold;
}

// static
int PanelDragController::GetGluePanelOverlapThresholdForTesting() {
  return kGluePanelsOverlapThreshold;
}

PanelDragController::PanelDragController(PanelManager* panel_manager)
    : panel_manager_(panel_manager),
      panel_stacking_enabled_(PanelManager::IsPanelStackingEnabled()),
      dragging_panel_(NULL),
      dragging_panel_original_collection_(NULL) {
}

PanelDragController::~PanelDragController() {
}

void PanelDragController::StartDragging(Panel* panel,
                                        const gfx::Point& mouse_location) {
  DCHECK(!dragging_panel_);

  offset_from_mouse_location_on_drag_start_ =
      mouse_location - panel->GetBounds().origin();

  dragging_panel_ = panel;
  SetPreviewModeForPanelAndBelow(dragging_panel_, true);

  // Keep track of original collection and placement for the case that the drag
  // is cancelled.
  dragging_panel_original_collection_ = dragging_panel_->collection();
  dragging_panel_original_collection_->SavePanelPlacement(dragging_panel_);
}

void PanelDragController::Drag(const gfx::Point& mouse_location) {
  if (!dragging_panel_)
    return;

  gfx::Point target_position = GetPanelPositionForMouseLocation(mouse_location);

  if (panel_stacking_enabled_) {
    // Check if the dragging panel can be moved out the stack. Note that this
    // has to be done first and we should continue processing it for the case
    // that the drag also triggers stacking and docking.
    // Note that the panel can only be unstacked from top or bottom. So if
    // unstacking from top succeeds, there is no need to check for unstacking
    // from bottom.
    if (!TryUnstackFromTop(target_position))
      TryUnstackFromBottom(target_position);

    // Check if the dragging panel can stack with other panel or stack.
    TryStack(target_position);
  }

  // Check if the dragging panel can be docked.
  TryDock(target_position);

  // Check if the dragging panel can be detached.
  TryDetach(target_position);

  // Check if the dragging panel can snap to other panel.
  if (panel_stacking_enabled_)
    TrySnap(&target_position);

  // At last, handle the drag via its collection's specific handler.
  switch (dragging_panel_->collection()->type()) {
    case PanelCollection::DOCKED:
      DockedPanelDragHandler::HandleDrag(dragging_panel_, target_position);
      break;
    case PanelCollection::DETACHED:
      DetachedPanelDragHandler::HandleDrag(dragging_panel_, target_position);
      break;
    case PanelCollection::STACKED:
      StackedPanelDragHandler::HandleDrag(
          dragging_panel_,
          target_position,
          dragging_panel_->collection() == dragging_panel_original_collection_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void PanelDragController::EndDragging(bool cancelled) {
  if (!dragging_panel_)
    return;

  PanelCollection* current_collection = dragging_panel_->collection();
  if (cancelled) {
    // Restore the dragging panel to its original collection if needed.
    // Note that the bounds of dragging panel is updated later by calling
    // RestorePanelToSavedPlacement.
    if (current_collection != dragging_panel_original_collection_) {
      PanelCollection::PositioningMask positioning_mask =
          static_cast<PanelCollection::PositioningMask>(
              PanelCollection::DEFAULT_POSITION |
              PanelCollection::DO_NOT_UPDATE_BOUNDS);
      MovePanelAndBelowToCollection(dragging_panel_,
                                    dragging_panel_original_collection_,
                                    positioning_mask);
    }

    // End the preview mode.
    SetPreviewModeForPanelAndBelow(dragging_panel_, false);

    // Restore the dragging panel to its original placement.
    dragging_panel_original_collection_->RestorePanelToSavedPlacement();
  } else {
    // The saved placement is no longer needed.
    dragging_panel_original_collection_->DiscardSavedPanelPlacement();

    // Finalizing the drag is only needed for the stack.
    if (current_collection->type() == PanelCollection::STACKED)
      StackedPanelDragHandler::FinalizeDrag(dragging_panel_);

    // End the preview mode.
    SetPreviewModeForPanelAndBelow(dragging_panel_, false);

    // This could cause the panel to be moved to its finalized position.
    current_collection->RefreshLayout();
  }

  // If the origianl collection is a stack and it becomes empty, remove it.
  if (dragging_panel_original_collection_->type() == PanelCollection::STACKED) {
    StackedPanelCollection* original_stack =
        static_cast<StackedPanelCollection*>(
            dragging_panel_original_collection_);
    if (original_stack->num_panels() == 0)
      panel_manager_->RemoveStack(original_stack);
  }

  dragging_panel_ = NULL;
}

void PanelDragController::OnPanelClosed(Panel* panel) {
  // Abort the drag only if the panel being closed is currently being dragged.
  if (dragging_panel_ != panel)
    return;

  dragging_panel_original_collection_->DiscardSavedPanelPlacement();
  dragging_panel_original_collection_ = NULL;
  dragging_panel_ = NULL;
}

gfx::Point PanelDragController::GetPanelPositionForMouseLocation(
    const gfx::Point& mouse_location) const {
  // The target panel position is computed based on the fact that the panel
  // should follow the mouse movement.
  gfx::Point target_position =
      mouse_location - offset_from_mouse_location_on_drag_start_;

  // If the mouse is within the main screen area, make sure that the top
  // border of panel cannot go outside the work area. This is to prevent
  // panel's titlebar from being moved under the taskbar or OSX menu bar
  // that is aligned to top screen edge.
  int display_area_top_position = panel_manager_->display_area().y();
  if (panel_manager_->display_settings_provider()->
          GetPrimaryScreenArea().Contains(mouse_location) &&
      target_position.y() < display_area_top_position) {
    target_position.set_y(display_area_top_position);
  }

  return target_position;
}

void PanelDragController::TryDetach(const gfx::Point& target_position) {
  // It has to come from the docked collection.
  if (dragging_panel_->collection()->type() != PanelCollection::DOCKED)
    return;

  // The minimized docked panel is not allowed to detach.
  if (dragging_panel_->IsMinimized())
    return;

  // Panels in the detached collection are always at their full size.
  gfx::Rect target_bounds(target_position,
                                dragging_panel_->full_size());

  // The panel should be dragged up high enough to pass certain threshold.
  if (panel_manager_->docked_collection()->display_area().bottom() -
          target_bounds.bottom() < kDetachDockedPanelThreshold)
    return;

  // Apply new panel bounds.
  dragging_panel_->SetPanelBoundsInstantly(target_bounds);

  // Move the panel to new collection.
  panel_manager_->MovePanelToCollection(dragging_panel_,
                                        panel_manager_->detached_collection(),
                                        PanelCollection::KNOWN_POSITION);
}

void PanelDragController::TryDock(const gfx::Point& target_position) {
  // It has to come from the detached collection.
  if (dragging_panel_->collection()->type() != PanelCollection::DETACHED)
    return;

  gfx::Rect target_bounds(target_position,
                                dragging_panel_->GetBounds().size());

  // If the target panel bounds is outside the main display area where the
  // docked collection resides, as in the multi-monitor scenario, we want it to
  // be still free-floating.
  gfx::Rect display_area = panel_manager_->display_settings_provider()->
      GetDisplayArea();
  if (!display_area.Intersects(target_bounds))
    return;

  // The bottom of the panel should come very close to or fall below the bottom
  // of the docked area.
  if (panel_manager_->docked_collection()->display_area().bottom() -
         target_bounds.bottom() > kDockDetachedPanelThreshold)
    return;

  // Apply new panel bounds.
  dragging_panel_->SetPanelBoundsInstantly(target_bounds);

  // Move the panel to new collection.
  panel_manager_->MovePanelToCollection(dragging_panel_,
                                        panel_manager_->docked_collection(),
                                        PanelCollection::KNOWN_POSITION);
}

void PanelDragController::TryStack(const gfx::Point& target_position) {
  gfx::Rect target_bounds;
  GlueEdge target_edge;
  Panel* target_panel = FindPanelToGlue(target_position,
                                        STACK,
                                        &target_bounds,
                                        &target_edge);
  if (!target_panel)
    return;

  StackedPanelCollection* dragging_stack = dragging_panel_->stack();

  // Move the panel (and all the panels below if in a stack) to the new
  // position.
  gfx::Vector2d delta =
      target_bounds.origin() - dragging_panel_->GetBounds().origin();
  if (dragging_stack)
    dragging_stack->MoveAllDraggingPanelsInstantly(delta);
  else
    dragging_panel_->MoveByInstantly(delta);

  // If the panel to stack with is not in a stack, create it now.
  StackedPanelCollection* target_stack = target_panel->stack();
  if (!target_stack) {
    target_stack = panel_manager_->CreateStack();
    panel_manager_->MovePanelToCollection(target_panel,
                                          target_stack,
                                          PanelCollection::DEFAULT_POSITION);
  }

  // Move the panel to new collection.
  // Note that we don't want to refresh the layout now because when we add
  // a panel to top of other panel, we don't want the bottom panel to change
  // its width to be same as top panel now.
  PanelCollection::PositioningMask positioning_mask =
      static_cast<PanelCollection::PositioningMask>(
          PanelCollection::NO_LAYOUT_REFRESH |
          (target_edge == TOP_EDGE ? PanelCollection::TOP_POSITION
                                   : PanelCollection::DEFAULT_POSITION));
  MovePanelAndBelowToCollection(dragging_panel_,
                                target_stack,
                                positioning_mask);
}

// Check if a panel or a set of stacked panels (being dragged together from a
// stack) can be dragged away from the panel below such that the former panel(s)
// are not in the same stack as the latter panel.
bool PanelDragController::TryUnstackFromTop(const gfx::Point& target_position) {
  // It has to be stacked.
  StackedPanelCollection* dragging_stack = dragging_panel_->stack();
  if (!dragging_stack)
    return false;

  // Unstacking from top only happens when a panel/stack stacks to the top of
  // another panel and then moves away while the drag is still in progress.
  if (dragging_panel_ != dragging_stack->top_panel() ||
      dragging_stack == dragging_panel_original_collection_)
    return false;

  // Count the number of panels that might need to unstack.
  Panel* last_panel_to_unstack = NULL;
  Panel* panel_below_last_panel_to_unstack = NULL;
  int num_panels_to_unstack = 0;
  for (StackedPanelCollection::Panels::const_iterator iter =
           dragging_stack->panels().begin();
       iter != dragging_stack->panels().end(); ++iter) {
    if (!(*iter)->in_preview_mode()) {
      panel_below_last_panel_to_unstack = *iter;
      break;
    }
    num_panels_to_unstack++;
    last_panel_to_unstack = *iter;
  }
  DCHECK_GE(num_panels_to_unstack, 1);
  if (num_panels_to_unstack == dragging_stack->num_panels())
    return false;

  gfx::Vector2d delta = target_position - dragging_panel_->GetBounds().origin();

  // The last panel to unstack should be dragged far enough from its below
  // panel.
  gfx::Rect target_bounds = last_panel_to_unstack->GetBounds();
  target_bounds.Offset(delta);
  gfx::Rect below_panel_bounds = panel_below_last_panel_to_unstack->GetBounds();
  if (GetVerticalDistance(target_bounds, below_panel_bounds) <
          kGluePanelsDistanceThreshold &&
      GetHorizontalOverlap(target_bounds, below_panel_bounds) >
          kGluePanelsOverlapThreshold) {
    return false;
  }

  // Move the panel (and all the panels below if in a stack) to the new
  // position.
  for (StackedPanelCollection::Panels::const_iterator iter =
           dragging_stack->panels().begin();
       iter != dragging_stack->panels().end(); ++iter) {
    if (!(*iter)->in_preview_mode())
      break;
    (*iter)->MoveByInstantly(delta);
  }

  int num_panels_in_stack = dragging_stack->num_panels();
  DCHECK_GE(num_panels_in_stack, 2);

  // When a panel is removed from its stack, we always make it detached. If it
  // indeed should go to the docked collection, the subsequent TryDock will then
  // move it from the detached collection to the docked collection.
  DetachedPanelCollection* detached_collection =
      panel_manager_->detached_collection();

  // If there're only 2 panels in the stack, both panels should move out of the
  // stack and the stack should be removed.
  if (num_panels_in_stack == 2) {
    DCHECK_EQ(1, num_panels_to_unstack);
    MovePanelAndBelowToCollection(dragging_panel_,
                                  detached_collection,
                                  PanelCollection::KNOWN_POSITION);
    return true;
  }

  DCHECK_GE(num_panels_in_stack, 3);

  // If only one panel (top panel) needs to unstack, move it out of the stack.
  if (num_panels_to_unstack == 1) {
    panel_manager_->MovePanelToCollection(dragging_panel_,
                                          panel_manager_->detached_collection(),
                                          PanelCollection::KNOWN_POSITION);
    return true;
  }

  // If all the panels except the bottom panel need to unstack, simply move
  // bottom panel out of the stack.
  if (num_panels_in_stack - num_panels_to_unstack == 1) {
    panel_manager_->MovePanelToCollection(dragging_stack->bottom_panel(),
                                          panel_manager_->detached_collection(),
                                          PanelCollection::KNOWN_POSITION);
    return true;
  }

  // Otherwise, move all unstacked panels to a new stack.
  // Note that all the panels to move should be copied to a local list first
  // because the stack collection will be modified during the move.
  std::list<Panel*> panels_to_move;
  for (StackedPanelCollection::Panels::const_iterator iter =
           dragging_stack->panels().begin();
       iter != dragging_stack->panels().end(); ++iter) {
    Panel* panel = *iter;
    if (!panel->in_preview_mode())
      break;
    panels_to_move.push_back(panel);
  }
  StackedPanelCollection* new_stack = panel_manager_->CreateStack();
  for (std::list<Panel*>::const_iterator iter = panels_to_move.begin();
       iter != panels_to_move.end(); ++iter) {
    panel_manager_->MovePanelToCollection(*iter,
                                          new_stack,
                                          PanelCollection::KNOWN_POSITION);
  }

  return true;
}

// Check if a panel or a set of stacked panels (being dragged together from a
// stack) can be dragged away from the panel above such that the former panel(s)
// are not in the same stack as the latter panel.
bool PanelDragController::TryUnstackFromBottom(
    const gfx::Point& target_position) {
  // It has to be stacked.
  StackedPanelCollection* dragging_stack = dragging_panel_->stack();
  if (!dragging_stack)
    return false;

  // It cannot be the top panel.
  if (dragging_panel_ == dragging_stack->top_panel())
    return false;

  DCHECK_GT(dragging_stack->num_panels(), 1);

  gfx::Rect target_bounds(target_position, dragging_panel_->GetBounds().size());

  // The panel should be dragged far enough from its above panel.
  Panel* above_panel = dragging_stack->GetPanelAbove(dragging_panel_);
  DCHECK(above_panel);
  gfx::Rect above_panel_bounds = above_panel->GetBounds();
  if (GetVerticalDistance(above_panel_bounds, target_bounds) <
          kGluePanelsDistanceThreshold &&
      GetHorizontalOverlap(above_panel_bounds, target_bounds) >
          kGluePanelsOverlapThreshold) {
    return false;
  }

  // Move the panel (and all the panels below if in a stack) to the new
  // position.
  gfx::Vector2d delta = target_position - dragging_panel_->GetBounds().origin();
  if (dragging_stack)
    dragging_stack->MoveAllDraggingPanelsInstantly(delta);
  else
    dragging_panel_->MoveByInstantly(delta);

  // If there're only 2 panels in the stack, both panels should move out the
  // stack and the stack should be removed.
  DetachedPanelCollection* detached_collection =
      panel_manager_->detached_collection();
  if (dragging_stack->num_panels() == 2) {
    MovePanelAndBelowToCollection(dragging_stack->top_panel(),
                                  detached_collection,
                                  PanelCollection::KNOWN_POSITION);
    return true;
  }

  // There're at least 3 panels.
  DCHECK_GE(dragging_stack->num_panels(), 3);

  // If the dragging panel is bottom panel, move it out of the stack.
  if (dragging_panel_ == dragging_stack->bottom_panel()) {
    panel_manager_->MovePanelToCollection(dragging_panel_,
                                          detached_collection,
                                          PanelCollection::KNOWN_POSITION);
    return true;
  }

  // If the dragging panel is the one below the top panel, move top panel
  // out of the stack.
  if (dragging_stack->GetPanelAbove(dragging_panel_) ==
      dragging_stack->top_panel()) {
    panel_manager_->MovePanelToCollection(dragging_stack->top_panel(),
                                          detached_collection,
                                          PanelCollection::KNOWN_POSITION);
    return true;
  }

  // There're at least 4 panels.
  DCHECK_GE(dragging_stack->num_panels(), 4);

  // We can split them into 2 stacks by moving the dragging panel and all panels
  // below to a new stack while keeping all panels above in the same stack.
  StackedPanelCollection* new_stack = panel_manager_->CreateStack();
  MovePanelAndBelowToCollection(dragging_panel_,
                                new_stack,
                                PanelCollection::KNOWN_POSITION);

  return true;
}

void PanelDragController::TrySnap(gfx::Point* target_position) {
  // Snapping does not apply to docked panels.
  if (dragging_panel_->collection()->type() == PanelCollection::DOCKED)
    return;

  gfx::Rect target_bounds;
  GlueEdge target_edge;
  Panel* target_panel = FindPanelToGlue(*target_position,
                                        SNAP,
                                        &target_bounds,
                                        &target_edge);
  if (!target_panel)
    return;

  *target_position = target_bounds.origin();
}

Panel* PanelDragController::FindPanelToGlue(
    const gfx::Point& potential_position,
    GlueAction action,
    gfx::Rect* target_bounds,
    GlueEdge* target_edge) const {
  int best_distance = kint32max;
  Panel* best_matching_panel = NULL;

  // Compute the potential bounds for the dragging panel.
  gfx::Rect current_dragging_bounds = dragging_panel_->GetBounds();
  gfx::Rect potential_dragging_bounds(potential_position,
                                      current_dragging_bounds.size());

  // Compute the potential bounds for the bottom panel if the dragging panel is
  // in a stack. If not, it is same as |potential_dragging_bounds|.
  // This is used to determine if the dragging panel or the bottom panel can
  // stack to the top of other panel.
  gfx::Rect current_bottom_bounds;
  gfx::Rect potential_bottom_bounds;
  StackedPanelCollection* dragging_stack = dragging_panel_->stack();
  if (dragging_stack && dragging_panel_ != dragging_stack->bottom_panel()) {
    gfx::Vector2d delta = potential_position - current_dragging_bounds.origin();
    current_bottom_bounds = dragging_stack->bottom_panel()->GetBounds();
    potential_bottom_bounds = current_bottom_bounds;
    potential_bottom_bounds.Offset(delta);
  } else {
    current_bottom_bounds = current_dragging_bounds;
    potential_bottom_bounds = potential_dragging_bounds;
  }

  // Go through all non-docked panels.
  std::vector<Panel*> panels = panel_manager_->GetDetachedAndStackedPanels();
  for (std::vector<Panel*>::const_iterator iter = panels.begin();
       iter != panels.end(); ++iter) {
    Panel* panel = *iter;
    if (dragging_panel_ == panel)
      continue;
    if (dragging_panel_->collection()->type() == PanelCollection::STACKED &&
        dragging_panel_->collection() == panel->collection())
      continue;
    gfx::Rect panel_bounds = panel->GetBounds();
    int distance;
    int overlap;

    if (action == SNAP) {
      overlap = GetVerticalOverlap(potential_dragging_bounds, panel_bounds);
      if (overlap > kGluePanelsOverlapThreshold) {
        // Can |dragging_panel_| snap to left edge of |panel|?
        distance = GetHorizontalDistance(potential_dragging_bounds,
                                         panel_bounds);
        if (distance < kGluePanelsDistanceThreshold &&
            distance < best_distance) {
          best_distance = distance;
          best_matching_panel = panel;
          *target_edge = LEFT_EDGE;
          *target_bounds = potential_dragging_bounds;
          target_bounds->set_x(panel_bounds.x() - target_bounds->width());
        }

        // Can |dragging_panel_| snap to right edge of |panel|?
        distance = GetHorizontalDistance(panel_bounds,
                                         potential_dragging_bounds);
        if (distance < kGluePanelsDistanceThreshold &&
            distance < best_distance) {
          best_distance = distance;
          best_matching_panel = panel;
          *target_edge = RIGHT_EDGE;
          *target_bounds = potential_dragging_bounds;
          target_bounds->set_x(panel_bounds.right());
        }
      }
    } else {
      DCHECK_EQ(STACK, action);

      // Can |dragging_panel_| or the bottom panel in |dragging_panel_|'s stack
      // stack to top edge of |panel|?
      distance = GetVerticalDistance(potential_bottom_bounds, panel_bounds);
      overlap = GetHorizontalOverlap(panel_bounds, potential_bottom_bounds);
      if (distance < kGluePanelsDistanceThreshold &&
          overlap > kGluePanelsOverlapThreshold &&
          distance < best_distance) {
        best_distance = distance;
        best_matching_panel = panel;
        *target_edge = TOP_EDGE;
        target_bounds->SetRect(
            potential_dragging_bounds.x(),
            current_dragging_bounds.y() + panel_bounds.y() -
                current_bottom_bounds.height() - current_bottom_bounds.y(),
            potential_dragging_bounds.width(),
            potential_dragging_bounds.height());
      }

      // Can |dragging_panel_| stack to bottom edge of |panel|?
      distance = GetVerticalDistance(panel_bounds, potential_dragging_bounds);
      overlap = GetHorizontalOverlap(panel_bounds, potential_dragging_bounds);
      if (distance < kGluePanelsDistanceThreshold &&
          overlap > kGluePanelsOverlapThreshold &&
          distance < best_distance) {
        best_distance = distance;
        best_matching_panel = panel;
        *target_edge = BOTTOM_EDGE;
        target_bounds->SetRect(potential_dragging_bounds.x(),
                               panel_bounds.bottom(),
                               potential_dragging_bounds.width(),
                               potential_dragging_bounds.height());
      }
    }
  }

  return best_matching_panel;
}

void PanelDragController::MovePanelAndBelowToCollection(
    Panel* panel,
    PanelCollection* target_collection,
    PanelCollection::PositioningMask positioning_mask) const {
  StackedPanelCollection* stack = panel->stack();
  if (!stack) {
    panel_manager_->MovePanelToCollection(panel,
                                          target_collection,
                                          positioning_mask);
    return;
  }

  // Note that all the panels to move should be copied to a local list first
  // because the stack collection will be modified during the move.
  std::list<Panel*> panels_to_move;
  StackedPanelCollection::Panels::const_iterator iter = stack->panels().begin();
  for (; iter != stack->panels().end(); ++iter)
    if ((*iter) == panel)
      break;
  for (; iter != stack->panels().end(); ++iter) {
    // Note that if the panels are going to be inserted from the top, we need
    // to reverse the order when copying to the local list.
    if (positioning_mask & PanelCollection::TOP_POSITION)
      panels_to_move.push_front(*iter);
    else
      panels_to_move.push_back(*iter);
  }
  for (std::list<Panel*>::const_iterator panel_iter = panels_to_move.begin();
       panel_iter != panels_to_move.end(); ++panel_iter) {
    panel_manager_->MovePanelToCollection(*panel_iter,
                                          target_collection,
                                          positioning_mask);
  }

  // If the stack becomes empty or has only one panel left, no need to keep
  // the stack.
  if (stack && stack->num_panels() <= 1) {
    if (stack->num_panels() == 1) {
      panel_manager_->MovePanelToCollection(
          stack->top_panel(),
          panel_manager_->detached_collection(),
          PanelCollection::KNOWN_POSITION);
    }
    // Note that if the stack is the original collection, do not remove it now.
    // This is because the original collection contains the information to
    // restore the dragging panel to the right place when the drag is cancelled.
    if (stack != dragging_panel_original_collection_)
      panel_manager_->RemoveStack(stack);
  }
}
