// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_resize_controller.h"

#include "base/logging.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"

namespace {
  static bool ResizingLeft(panel::ResizingSides sides) {
    return sides == panel::RESIZE_TOP_LEFT ||
           sides == panel::RESIZE_LEFT ||
           sides == panel::RESIZE_BOTTOM_LEFT;
  }

  static bool ResizingRight(panel::ResizingSides sides) {
    return sides == panel::RESIZE_TOP_RIGHT ||
           sides == panel::RESIZE_RIGHT ||
           sides == panel::RESIZE_BOTTOM_RIGHT;
  }

  static bool ResizingTop(panel::ResizingSides sides) {
    return sides == panel::RESIZE_TOP_LEFT ||
           sides == panel::RESIZE_TOP ||
           sides == panel::RESIZE_TOP_RIGHT;
  }

  static bool ResizingBottom(panel::ResizingSides sides) {
    return sides == panel::RESIZE_BOTTOM_RIGHT ||
           sides == panel::RESIZE_BOTTOM ||
           sides == panel::RESIZE_BOTTOM_LEFT;
  }
}

PanelResizeController::PanelResizeController(PanelManager* panel_manager)
  : panel_manager_(panel_manager),
    resizing_panel_(NULL),
    sides_resized_(panel::RESIZE_NONE) {
}

void PanelResizeController::StartResizing(Panel* panel,
                                          const gfx::Point& mouse_location,
                                          panel::ResizingSides sides) {
  DCHECK(!IsResizing());
  DCHECK_NE(panel::RESIZE_NONE, sides);

  panel::Resizability resizability = panel->CanResizeByMouse();
  DCHECK_NE(panel::NOT_RESIZABLE, resizability);
  panel::Resizability resizability_to_test;
  switch (sides) {
    case panel::RESIZE_TOP_LEFT:
      resizability_to_test = panel::RESIZABLE_TOP_LEFT;
      break;
    case panel::RESIZE_TOP:
      resizability_to_test = panel::RESIZABLE_TOP;
      break;
    case panel::RESIZE_TOP_RIGHT:
      resizability_to_test = panel::RESIZABLE_TOP_RIGHT;
      break;
    case panel::RESIZE_LEFT:
      resizability_to_test = panel::RESIZABLE_LEFT;
      break;
    case panel::RESIZE_RIGHT:
      resizability_to_test = panel::RESIZABLE_RIGHT;
      break;
    case panel::RESIZE_BOTTOM_LEFT:
      resizability_to_test = panel::RESIZABLE_BOTTOM_LEFT;
      break;
    case panel::RESIZE_BOTTOM:
      resizability_to_test = panel::RESIZABLE_BOTTOM;
      break;
    case panel::RESIZE_BOTTOM_RIGHT:
      resizability_to_test = panel::RESIZABLE_BOTTOM_RIGHT;
      break;
    default:
      resizability_to_test = panel::NOT_RESIZABLE;
      break;
  }
  if ((resizability & resizability_to_test) == 0) {
    DLOG(WARNING) << "Resizing not allowed. Is this a test?";
    return;
  }

  mouse_location_at_start_ = mouse_location;
  bounds_at_start_ = panel->GetBounds();
  sides_resized_ = sides;
  resizing_panel_ = panel;
  resizing_panel_->OnPanelStartUserResizing();
}

void PanelResizeController::Resize(const gfx::Point& mouse_location) {
  DCHECK(IsResizing());
  panel::Resizability resizability = resizing_panel_->CanResizeByMouse();
  if (panel::NOT_RESIZABLE == resizability) {
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
    int new_height = std::max(bounds_at_start_.height() +
                     mouse_location_at_start_.y() - mouse_location.y(), 0);
    int new_y = bounds_at_start_.bottom() - new_height;

    // Make sure that the panel's titlebar cannot be resized under the taskbar
    // or OSX menu bar that is aligned to top screen edge.
    gfx::Rect display_area = panel_manager_->display_settings_provider()->
        GetDisplayAreaMatching(bounds);
    gfx::Rect work_area = panel_manager_->display_settings_provider()->
        GetWorkAreaMatching(bounds);
    if (display_area.y() <= mouse_location.y() &&
        mouse_location.y() < work_area.y()) {
      new_height -= work_area.y() - new_y;
    }

    bounds.set_height(new_height);
  }

  resizing_panel_->IncreaseMaxSize(bounds.size());

  // This effectively only clamps using the min size, since the max_size was
  // updated above.
  bounds.set_size(resizing_panel_->ClampSize(bounds.size()));

  if (ResizingLeft(sides_resized_))
    bounds.set_x(bounds_at_start_.right() - bounds.width());

  if (ResizingTop(sides_resized_))
    bounds.set_y(bounds_at_start_.bottom() - bounds.height());

  if (bounds != resizing_panel_->GetBounds()) {
    resizing_panel_->SetPanelBoundsInstantly(bounds);
    resizing_panel_->OnWindowResizedByMouse(bounds);
  }
}

Panel* PanelResizeController::EndResizing(bool cancelled) {
  DCHECK(IsResizing());

  if (cancelled) {
    resizing_panel_->SetPanelBoundsInstantly(bounds_at_start_);
    resizing_panel_->OnWindowResizedByMouse(bounds_at_start_);
  }

  // Do a thorough cleanup.
  resizing_panel_->OnPanelEndUserResizing();
  Panel* resized_panel = resizing_panel_;
  resizing_panel_ = NULL;
  sides_resized_ = panel::RESIZE_NONE;
  bounds_at_start_ = gfx::Rect();
  mouse_location_at_start_ = gfx::Point();
  return resized_panel;
}

void PanelResizeController::OnPanelClosed(Panel* panel) {
  if (!resizing_panel_)
    return;

  // If the resizing panel is closed, abort the resize operation.
  if (resizing_panel_ == panel)
    EndResizing(false);
}
