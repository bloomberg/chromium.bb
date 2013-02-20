// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panel_window_event_filter.h"

#include "ash/wm/window_util.h"
#include "base/message_loop.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/events/event.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

PanelWindowEventFilter::PanelWindowEventFilter(
    aura::Window* panel_container,
    PanelLayoutManager* layout_manager)
    : panel_container_(panel_container),
      layout_manager_(layout_manager),
      dragged_panel_(NULL),
      drag_state_(DRAG_NONE) {
  panel_container_->AddObserver(this);
}

PanelWindowEventFilter::~PanelWindowEventFilter() {
  panel_container_->RemoveObserver(this);
  panel_container_->RemovePreTargetHandler(this);
}

void PanelWindowEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED: {
      int hitResult = target->delegate()->
          GetNonClientComponent(event->location());
      if (hitResult == HTCAPTION) {
        if (drag_state_ == DRAG_NONE) {
          dragged_panel_ = target;
          drag_location_in_dragged_window_ = event->location();
          drag_state_ = DRAG_CLICKED;
          event->StopPropagation();
        }
      }
      return;
    }

    case ui::ET_MOUSE_DRAGGED:
      if (drag_state_ == DRAG_CLICKED) {
        drag_state_ = DRAG_STARTED;
        layout_manager_->StartDragging(dragged_panel_);
      }
      if (drag_state_ == DRAG_STARTED && HandleDrag(target, event))
        event->StopPropagation();
      return;

    case ui::ET_MOUSE_CAPTURE_CHANGED:
      if (drag_state_ == DRAG_STARTED) {
        FinishDrag();
        event->StopPropagation();
      } else if (drag_state_ == DRAG_CLICKED) {
        drag_state_ = DRAG_NONE;
        dragged_panel_ = NULL;
        event->StopPropagation();
      }
      return;

    case ui::ET_MOUSE_RELEASED:
      if (drag_state_ == DRAG_STARTED) {
        FinishDrag();
        event->StopPropagation();
      } else if (dragged_panel_ != NULL) {
        drag_state_ = DRAG_NONE;
        dragged_panel_ = NULL;
        event->StopPropagation();
      }
      return;
    default:
      return;
  }
}

void PanelWindowEventFilter::OnWindowDestroying(aura::Window* window) {
  CHECK_EQ(panel_container_, window);
  delete this;
}

bool PanelWindowEventFilter::HandleDrag(aura::Window* target,
                                        ui::LocatedEvent* event) {
  gfx::Rect target_bounds = dragged_panel_->bounds();
  gfx::Point event_location_in_parent(event->location());
  aura::Window::ConvertPointToTarget(target,
                                     target->parent(),
                                     &event_location_in_parent);
  target_bounds.set_x(
      event_location_in_parent.x() - drag_location_in_dragged_window_.x());
  dragged_panel_->SetBounds(target_bounds);
  return true;
}

void PanelWindowEventFilter::FinishDrag() {
  layout_manager_->FinishDragging();
  drag_state_ = DRAG_NONE;
  dragged_panel_ = NULL;
}

}
}
