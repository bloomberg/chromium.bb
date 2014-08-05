// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_bezel_event_filter.h"

#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ui/aura/window.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

ShelfBezelEventFilter::ShelfBezelEventFilter(
    ShelfLayoutManager* shelf)
    : shelf_(shelf),
      in_touch_drag_(false) {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

ShelfBezelEventFilter::~ShelfBezelEventFilter() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void ShelfBezelEventFilter::OnGestureEvent(
    ui::GestureEvent* event) {
  gfx::Point point_in_screen(event->location());
  aura::Window* target = static_cast<aura::Window*>(event->target());
  ::wm::ConvertPointToScreen(target, &point_in_screen);
  gfx::Rect screen =
      Shell::GetScreen()->GetDisplayNearestPoint(point_in_screen).bounds();
  if ((!screen.Contains(point_in_screen) &&
       IsShelfOnBezel(screen, point_in_screen)) ||
      in_touch_drag_) {
    if (gesture_handler_.ProcessGestureEvent(*event)) {
      switch (event->type()) {
        case ui::ET_GESTURE_SCROLL_BEGIN:
          in_touch_drag_ = true;
          break;
        case ui::ET_GESTURE_SCROLL_END:
        case ui::ET_SCROLL_FLING_START:
          in_touch_drag_ = false;
          break;
        default:
          break;
      }
      event->StopPropagation();
    }
  }
}

bool ShelfBezelEventFilter::IsShelfOnBezel(
    const gfx::Rect& screen,
    const gfx::Point& point) const{
  switch (shelf_->GetAlignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
      if (point.y() >= screen.bottom())
        return true;
      break;
    case SHELF_ALIGNMENT_LEFT:
      if (point.x() <= screen.x())
        return true;
      break;
    case SHELF_ALIGNMENT_TOP:
      if (point.y() <= screen.y())
        return true;
      break;
    case SHELF_ALIGNMENT_RIGHT:
      if (point.x() >= screen.right())
        return true;
      break;
  }
  return false;
}

}  // namespace ash
