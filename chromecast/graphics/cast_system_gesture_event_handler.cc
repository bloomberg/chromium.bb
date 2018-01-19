// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_system_gesture_event_handler.h"

#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace chromecast {

namespace {

// The number of pixels from the very left or right of the screen to consider as
// a valid origin for the left or right swipe gesture.
constexpr int kSideGestureStartWidth = 10;

// The number of pixels from the very top or bottom of the screen to consider as
// a valid origin for the top or bottom swipe gesture.
constexpr int kSideGestureStartHeight = 10;

}  // namespace

CastSystemGestureEventHandler::CastSystemGestureEventHandler(
    aura::Window* root_window)
    : EventHandler(),
      root_window_(root_window),
      current_swipe_(CastSideSwipeOrigin::NONE) {
  DCHECK(root_window);
  root_window->AddPreTargetHandler(this);
}

CastSystemGestureEventHandler::~CastSystemGestureEventHandler() {
  DCHECK(swipe_gesture_handlers_.empty());
  root_window_->RemovePreTargetHandler(this);
}

CastSideSwipeOrigin CastSystemGestureEventHandler::GetDragPosition(
    const gfx::Point& point,
    const gfx::Rect& screen_bounds) const {
  if (point.y() < (screen_bounds.y() + kSideGestureStartHeight)) {
    return CastSideSwipeOrigin::TOP;
  }
  if (point.x() < (screen_bounds.x() + kSideGestureStartWidth)) {
    return CastSideSwipeOrigin::LEFT;
  }
  if (point.x() >
      (screen_bounds.x() + screen_bounds.width() - kSideGestureStartWidth)) {
    return CastSideSwipeOrigin::RIGHT;
  }
  if (point.y() >
      (screen_bounds.y() + screen_bounds.height() - kSideGestureStartHeight)) {
    return CastSideSwipeOrigin::BOTTOM;
  }
  return CastSideSwipeOrigin::NONE;
}

void CastSystemGestureEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  gfx::Point gesture_location(event->location());
  aura::Window* target = static_cast<aura::Window*>(event->target());

  // Convert the event's point to the point on the physical screen.
  // For cast on root window this is likely going to be identical, but put it
  // through the ScreenPositionClient just to be sure.
  ::wm::ConvertPointToScreen(target, &gesture_location);
  gfx::Rect screen_bounds = display::Screen::GetScreen()
                                ->GetDisplayNearestPoint(gesture_location)
                                .bounds();
  CastSideSwipeOrigin side_swipe_origin =
      GetDragPosition(gesture_location, screen_bounds);
  if (side_swipe_origin != CastSideSwipeOrigin::NONE ||
      current_swipe_ != CastSideSwipeOrigin::NONE) {
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_BEGIN:
        current_swipe_ = side_swipe_origin;
        for (auto* side_swipe_handler : swipe_gesture_handlers_) {
          side_swipe_handler->OnSideSwipeBegin(side_swipe_origin, event);
        }
        break;
      case ui::ET_GESTURE_SCROLL_END:
      case ui::ET_SCROLL_FLING_START:
        for (auto* side_swipe_handler : swipe_gesture_handlers_) {
          side_swipe_handler->OnSideSwipeEnd(current_swipe_, event);
        }
        current_swipe_ = CastSideSwipeOrigin::NONE;
        break;
      default:
        break;
    }
  }
}

void CastSystemGestureEventHandler::AddSideSwipeGestureHandler(
    CastSideSwipeGestureHandlerInterface* handler) {
  swipe_gesture_handlers_.push_back(handler);
}

void CastSystemGestureEventHandler::RemoveSideSwipeGestureHandler(
    CastSideSwipeGestureHandlerInterface* handler) {
  auto find = std::find(swipe_gesture_handlers_.begin(),
                        swipe_gesture_handlers_.end(), handler);
  if (find != swipe_gesture_handlers_.end())
    swipe_gesture_handlers_.erase(find);
}

}  // namespace chromecast
