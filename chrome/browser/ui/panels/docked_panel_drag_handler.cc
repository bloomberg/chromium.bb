// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/docked_panel_drag_handler.h"

#include "chrome/browser/ui/panels/docked_panel_collection.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

// static
void DockedPanelDragHandler::HandleDrag(Panel* panel,
                                        const gfx::Point& target_position) {
  DCHECK_EQ(PanelCollection::DOCKED, panel->collection()->type());

  DockedPanelCollection* collection =
      static_cast<DockedPanelCollection*>(panel->collection());

  // Moves this panel to the dragging position.
  // Note that we still allow the panel to be moved vertically until it gets
  // aligned to the bottom area.
  gfx::Rect new_bounds(panel->GetBounds());
  new_bounds.set_x(target_position.x());
  int delta_x = new_bounds.x() - panel->GetBounds().x();
  int bottom = collection->GetBottomPositionForExpansionState(
      panel->expansion_state());
  if (new_bounds.bottom() != bottom) {
    new_bounds.set_y(target_position.y());
    if (new_bounds.bottom() > bottom)
      new_bounds.set_y(bottom - new_bounds.height());
  }
  panel->SetPanelBoundsInstantly(new_bounds);

  if (delta_x) {
    // Checks and processes other affected panels.
    if (delta_x > 0)
      DragRight(panel);
    else
      DragLeft(panel);

    // Layout refresh will automatically recompute the bounds of all affected
    // panels due to their position changes.
    collection->RefreshLayout();
  }
}

// static
void DockedPanelDragHandler::DragLeft(Panel* panel) {
  DockedPanelCollection* collection =
      static_cast<DockedPanelCollection*>(panel->collection());

  // This is the left corner of the dragging panel. We use it to check against
  // all the panels on its left.
  int dragging_panel_left_boundary = panel->GetBounds().x();

  // Checks the panels to the left of the dragging panel.
  DockedPanelCollection::Panels::iterator dragging_panel_iterator =
      find(collection->panels_.begin(), collection->panels_.end(), panel);
  DockedPanelCollection::Panels::iterator current_panel_iterator =
      dragging_panel_iterator;
  ++current_panel_iterator;
  for (; current_panel_iterator != collection->panels_.end();
       ++current_panel_iterator) {
    Panel* current_panel = *current_panel_iterator;

    // Can we swap dragging panel with its left panel? The criterion is that
    // the left corner of dragging panel should pass the middle position of
    // its left panel.
    if (dragging_panel_left_boundary > current_panel->GetBounds().x() +
            current_panel->GetBounds().width() / 2)
      break;

    // Swaps the contents and makes |dragging_panel_iterator| refers to the new
    // position.
    *dragging_panel_iterator = current_panel;
    *current_panel_iterator = panel;
    dragging_panel_iterator = current_panel_iterator;
  }
}

// static
void DockedPanelDragHandler::DragRight(Panel* panel) {
  DockedPanelCollection* collection =
      static_cast<DockedPanelCollection*>(panel->collection());

  // This is the right corner of the dragging panel. We use it to check against
  // all the panels on its right.
  int dragging_panel_right_boundary = panel->GetBounds().x() +
      panel->GetBounds().width() - 1;

  // Checks the panels to the right of the dragging panel.
  DockedPanelCollection::Panels::iterator dragging_panel_iterator =
      find(collection->panels_.begin(), collection->panels_.end(), panel);
  DockedPanelCollection::Panels::iterator current_panel_iterator =
      dragging_panel_iterator;
  while (current_panel_iterator != collection->panels_.begin()) {
    current_panel_iterator--;
    Panel* current_panel = *current_panel_iterator;

    // Can we swap dragging panel with its right panel? The criterion is that
    // the left corner of dragging panel should pass the middle position of
    // its right panel.
    if (dragging_panel_right_boundary < current_panel->GetBounds().x() +
            current_panel->GetBounds().width() / 2)
      break;

    // Swaps the contents and makes |dragging_panel_iterator| refers to the new
    // position.
    *dragging_panel_iterator = current_panel;
    *current_panel_iterator = panel;
    dragging_panel_iterator = current_panel_iterator;
  }
}
