// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_drag_controller.h"

#include "base/logging.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_strip.h"

PanelDragController::PanelDragController()
    : dragging_panel_(NULL) {
}

PanelDragController::~PanelDragController() {
}

void PanelDragController::StartDragging(Panel* panel) {
  DCHECK(!dragging_panel_);
  DCHECK(panel->draggable());

  dragging_panel_ = panel;
  dragging_panel_original_position_ = panel->GetBounds().origin();

  dragging_panel_->panel_strip()->StartDraggingPanel(panel);
}

void PanelDragController::Drag(int delta_x, int delta_y) {
  DCHECK(dragging_panel_);

  dragging_panel_->panel_strip()->DragPanel(dragging_panel_, delta_x, delta_y);
}

void PanelDragController::EndDragging(bool cancelled) {
  DCHECK(dragging_panel_);

  // The code in PanelStrip::EndDraggingPanel might call DragController to find
  // out if the drag has ended. So we need to reset |dragging_panel_| to reflect
  // this.
  Panel* panel = dragging_panel_;
  dragging_panel_ = NULL;
  panel->panel_strip()->EndDraggingPanel(panel, cancelled);
}

void PanelDragController::OnPanelClosed(Panel* panel) {
  if (!dragging_panel_)
    return;

  // If the dragging panel is closed, abort the drag.
  if (dragging_panel_ == panel)
    EndDragging(false);
}
