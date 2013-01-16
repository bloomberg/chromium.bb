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
#include "chrome/browser/ui/panels/panel_collection.h"
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

}  // namespace

// static
int PanelDragController::GetDetachDockedPanelThresholdForTesting() {
  return kDetachDockedPanelThreshold;
}

// static
int PanelDragController::GetDockDetachedPanelThresholdForTesting() {
  return kDockDetachedPanelThreshold;
}

PanelDragController::PanelDragController(PanelManager* panel_manager)
    : panel_manager_(panel_manager),
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
  dragging_panel_->SetPreviewMode(true);

  // Keep track of original collection and placement for the case that the drag
  // is cancelled.
  dragging_panel_original_collection_ = dragging_panel_->collection();
  dragging_panel_original_collection_->SavePanelPlacement(dragging_panel_);
}

void PanelDragController::Drag(const gfx::Point& mouse_location) {
  if (!dragging_panel_)
    return;

  gfx::Point target_position = GetPanelPositionForMouseLocation(mouse_location);

  // Check if the dragging panel can be detached or docked.
  if (TryDetach(target_position))
    return;
  if (TryDock(target_position))
    return;

  // At last, handle the drag via its collection's specific handler.
  switch (dragging_panel_->collection()->type()) {
    case PanelCollection::DOCKED:
      DockedPanelDragHandler::HandleDrag(dragging_panel_, target_position);
      break;
    case PanelCollection::DETACHED:
      DetachedPanelDragHandler::HandleDrag(dragging_panel_, target_position);
      break;
    case PanelCollection::STACKED:
      StackedPanelDragHandler::HandleDrag(dragging_panel_, target_position);
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
      panel_manager_->MovePanelToCollection(
          dragging_panel_,
          dragging_panel_original_collection_,
          positioning_mask);
    }

    // End the preview mode.
    dragging_panel_->SetPreviewMode(false);

    // Restore the dragging panel to its original placement.
    dragging_panel_original_collection_->RestorePanelToSavedPlacement();
  } else {
    // The saved placement is no longer needed.
    dragging_panel_original_collection_->DiscardSavedPanelPlacement();

    // End the preview mode.
    dragging_panel_->SetPreviewMode(false);

    // This could cause the panel to be moved to its finalized position.
    current_collection->RefreshLayout();
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

bool PanelDragController::TryDetach(const gfx::Point& target_position) {
  // It has to come from the docked collection.
  if (dragging_panel_->collection()->type() != PanelCollection::DOCKED)
    return false;

  // The minimized docked panel is not allowed to detach.
  if (dragging_panel_->IsMinimized())
    return false;

  // Panels in the detached collection are always at their full size.
  gfx::Rect target_panel_bounds(target_position,
                                dragging_panel_->full_size());

  // The panel should be dragged up high enough to pass certain threshold.
  if (panel_manager_->docked_collection()->display_area().bottom() -
          target_panel_bounds.bottom() < kDetachDockedPanelThreshold)
    return false;

  // Apply new panel bounds.
  dragging_panel_->SetPanelBoundsInstantly(target_panel_bounds);

  // Move the panel to new collection.
  panel_manager_->MovePanelToCollection(dragging_panel_,
                                        panel_manager_->detached_collection(),
                                        PanelCollection::KNOWN_POSITION);

  return true;
}

bool PanelDragController::TryDock(const gfx::Point& target_position) {
  // It has to come from the detached collection.
  if (dragging_panel_->collection()->type() != PanelCollection::DETACHED)
    return false;

  gfx::Rect target_panel_bounds(target_position,
                                dragging_panel_->GetBounds().size());

  // If the target panel bounds is outside the main display area where the
  // docked collection resides, as in the multi-monitor scenario, we want it to
  // be still free-floating.
  gfx::Rect display_area = panel_manager_->display_settings_provider()->
      GetDisplayArea();
  if (!display_area.Intersects(target_panel_bounds))
    return false;

  // The bottom of the panel should come very close to or fall below the bottom
  // of the docked area.
  if (panel_manager_->docked_collection()->display_area().bottom() -
         target_panel_bounds.bottom() > kDockDetachedPanelThreshold)
    return false;

  // Apply new panel bounds.
  dragging_panel_->SetPanelBoundsInstantly(target_panel_bounds);

  // Move the panel to new collection.
  panel_manager_->MovePanelToCollection(dragging_panel_,
                                        panel_manager_->docked_collection(),
                                        PanelCollection::KNOWN_POSITION);

  return true;
}
