// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/stacked_panel_drag_handler.h"

#include "base/logging.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_collection.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

// static
void StackedPanelDragHandler::HandleDrag(Panel* panel,
                                         const gfx::Point& target_position,
                                         bool in_orginal_collection) {
  DCHECK_EQ(PanelCollection::STACKED, panel->collection()->type());

  StackedPanelCollection* stack = panel->stack();
  DCHECK(stack);

  // If the panel is in its original stack, only top panel is allowed to drag.
  if (in_orginal_collection && panel != stack->top_panel())
    return;

  // Find out if all panels in the stack are being dragged.
  bool all_panels_under_drag = true;
  for (StackedPanelCollection::Panels::const_iterator iter =
           stack->panels().begin();
       iter != stack->panels().end(); ++iter) {
    if (!(*iter)->in_preview_mode()) {
      all_panels_under_drag = false;
      break;
    }
  }

  gfx::Vector2d delta_origin = target_position - panel->GetBounds().origin();

  // If not all panels in the stack are being dragged, it means that these
  // panels being dragged have just been added to this stack. Dragging these
  // panels should only cause the horizontal movement due to that y position
  // of these panels have already aligned.
  if (!all_panels_under_drag)
    delta_origin.set_y(0);

  stack->MoveAllDraggingPanelsInstantly(delta_origin);
}

// static
void StackedPanelDragHandler::FinalizeDrag(Panel* panel) {
  DCHECK_EQ(PanelCollection::STACKED, panel->collection()->type());

  StackedPanelCollection* stack = panel->stack();
  DCHECK(stack);

  // It is only needed when dragging a panel/stack to stack to the top of
  // another panel/stack.
  if (stack->top_panel() != panel)
    return;

  // Find the first non-dragging panel that is used to align all dragging panels
  // above it to have the same x position. This is because all dragging panels
  // are only aligned vertically when the stacking occurs.
  Panel* first_non_dragging_panel = NULL;
  for (StackedPanelCollection::Panels::const_iterator iter =
           stack->panels().begin();
       iter != stack->panels().end(); ++iter) {
    Panel* panel = *iter;
    if (!panel->in_preview_mode()) {
      first_non_dragging_panel = panel;
      break;
    }
  }
  if (!first_non_dragging_panel)
    return;

  gfx::Vector2d delta_origin(
      first_non_dragging_panel->GetBounds().x() - panel->GetBounds().x(),
      0);
  stack->MoveAllDraggingPanelsInstantly(delta_origin);
}
