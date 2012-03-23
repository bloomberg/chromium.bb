// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_resize_controller.h"

#include "base/logging.h"
#include "chrome/browser/ui/panels/detached_panel_strip.h"
#include "chrome/browser/ui/panels/docked_panel_strip.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_strip.h"

namespace {
  static bool ResizingLeft(PanelResizeController::ResizingSides sides) {
    return sides == PanelResizeController::RESIZE_TOP_LEFT ||
           sides == PanelResizeController::RESIZE_LEFT ||
           sides == PanelResizeController::RESIZE_BOTTOM_LEFT;
  }

  static bool ResizingRight(PanelResizeController::ResizingSides sides) {
    return sides == PanelResizeController::RESIZE_TOP_RIGHT ||
           sides == PanelResizeController::RESIZE_RIGHT ||
           sides == PanelResizeController::RESIZE_BOTTOM_RIGHT;
  }

  static bool ResizingTop(PanelResizeController::ResizingSides sides) {
    return sides == PanelResizeController::RESIZE_TOP_LEFT ||
           sides == PanelResizeController::RESIZE_TOP ||
           sides == PanelResizeController::RESIZE_TOP_RIGHT;
  }

  static bool ResizingBottom(PanelResizeController::ResizingSides sides) {
    return sides == PanelResizeController::RESIZE_BOTTOM_RIGHT ||
           sides == PanelResizeController::RESIZE_BOTTOM ||
           sides == PanelResizeController::RESIZE_BOTTOM_LEFT;
  }
}

PanelResizeController::PanelResizeController(PanelManager* panel_manager)
  : panel_manager_(panel_manager),
    resizing_panel_(NULL),
    sides_resized_(NO_SIDES) {
}

void PanelResizeController::StartResizing(Panel* panel,
                                          const gfx::Point& mouse_location,
                                          ResizingSides sides) {
  DCHECK(!IsResizing());
  DCHECK(panel->panel_strip() &&
         panel->panel_strip()->CanResizePanel(panel));

  DCHECK_NE(NO_SIDES, sides);

  mouse_location_at_start_ = mouse_location;
  bounds_at_start_ = panel->GetBounds();
  sides_resized_ = sides;
  resizing_panel_ = panel;
}

void PanelResizeController::Resize(const gfx::Point& mouse_location) {
  DCHECK(IsResizing());
  if (resizing_panel_->panel_strip() == NULL ||
      !resizing_panel_->panel_strip()->CanResizePanel(resizing_panel_)) {
    EndResizing(false);
    return;
  }
  gfx::Rect bounds = resizing_panel_->GetBounds();

  if (ResizingRight(sides_resized_)) {
    bounds.set_width(std::max(bounds_at_start_.width() +
                     mouse_location.x() - mouse_location_at_start_.x(), 0));
  }
  if (ResizingBottom(sides_resized_)) {
    bounds.set_height(std::max(bounds_at_start_.height() +
                      mouse_location.y() - mouse_location_at_start_.y(), 0));
  }
  if (ResizingLeft(sides_resized_)) {
    bounds.set_width(std::max(bounds_at_start_.width() +
                     mouse_location_at_start_.x() - mouse_location.x(), 0));
  }
  if (ResizingTop(sides_resized_)) {
    bounds.set_height(std::max(bounds_at_start_.height() +
                      mouse_location_at_start_.y() - mouse_location.y(), 0));
  }

  // Give the panel a chance to adjust the bounds before setting them.
  gfx::Size size = bounds.size();
  resizing_panel_->ClampSize(&size);
  bounds.set_size(size);

  if (ResizingLeft(sides_resized_)) {
    bounds.set_x(bounds_at_start_.x() -
                 (bounds.width() - bounds_at_start_.width()));
  }

  if (ResizingTop(sides_resized_)) {
    bounds.set_y(bounds_at_start_.y() -
                 (bounds.height() - bounds_at_start_.height()));
  }

  if (bounds != resizing_panel_->GetBounds())
    panel_manager_->SetPanelBounds(resizing_panel_, bounds);
}

void PanelResizeController::EndResizing(bool cancelled) {
  DCHECK(IsResizing());

  if (cancelled) {
    panel_manager_->SetPanelBounds(resizing_panel_,
                                   bounds_at_start_);
  }

  // Do a thorough cleanup.
  resizing_panel_ = NULL;
  sides_resized_ = NO_SIDES;
  bounds_at_start_ = gfx::Rect();
  mouse_location_at_start_ = gfx::Point();
}

void PanelResizeController::OnPanelClosed(Panel* panel) {
  if (!resizing_panel_)
    return;

  // If the resizing panel is closed, abort the resize operation.
  if (resizing_panel_ == panel)
    EndResizing(false);
}

PanelResizeController::ResizingSides
    PanelResizeController::IsMouseNearFrameSide(
    gfx::Point mouse_location,
    int resize_edge_thickness,
    Panel* panel) {
  gfx::Rect bounds = panel->GetBounds();

  bool left = false, right = false, top = false, bottom = false;

  if (abs(mouse_location.x() - bounds.x()) <= resize_edge_thickness)
    left = true;
  if (abs(mouse_location.y() - bounds.y()) <= resize_edge_thickness)
    top = true;
  if (abs(mouse_location.x() - bounds.right()) <= resize_edge_thickness)
    right = true;
  if (abs(mouse_location.y() - bounds.bottom()) <= resize_edge_thickness)
    bottom = true;

  if (top && left)
    return RESIZE_TOP_LEFT;
  if (top && right)
    return RESIZE_TOP_RIGHT;
  if (bottom && left)
    return RESIZE_BOTTOM_LEFT;
  if (bottom && right)
    return RESIZE_BOTTOM_RIGHT;
  if (top)
    return RESIZE_TOP;
  if (bottom)
    return RESIZE_BOTTOM;
  if (left)
    return RESIZE_LEFT;
  if (right)
    return RESIZE_RIGHT;
  return NO_SIDES;
}
