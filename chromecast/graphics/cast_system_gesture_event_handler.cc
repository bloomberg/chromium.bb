// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_system_gesture_event_handler.h"

#include "chromecast/base/chromecast_switches.h"
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
constexpr int kDefaultSideGestureStartWidth = 10;

// The number of pixels from the very top or bottom of the screen to consider as
// a valid origin for the top or bottom swipe gesture.
constexpr int kDefaultSideGestureStartHeight = 10;

}  // namespace

CastSystemGestureEventHandler::CastSystemGestureEventHandler(
    aura::Window* root_window)
    : EventHandler(),
      gesture_start_width_(GetSwitchValueInt(switches::kSystemGestureStartWidth,
                                             kDefaultSideGestureStartWidth)),
      gesture_start_height_(
          GetSwitchValueInt(switches::kSystemGestureStartHeight,
                            kDefaultSideGestureStartHeight)),
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
  if (point.y() < (screen_bounds.y() + gesture_start_height_)) {
    return CastSideSwipeOrigin::TOP;
  }
  if (point.x() < (screen_bounds.x() + gesture_start_width_)) {
    return CastSideSwipeOrigin::LEFT;
  }
  if (point.x() >
      (screen_bounds.x() + screen_bounds.width() - gesture_start_width_)) {
    return CastSideSwipeOrigin::RIGHT;
  }
  if (point.y() >
      (screen_bounds.y() + screen_bounds.height() - gesture_start_height_)) {
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

  // Detect the beginning of a system gesture swipe.
  if (side_swipe_origin != CastSideSwipeOrigin::NONE &&
      event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    for (auto* side_swipe_handler : swipe_gesture_handlers_) {
      // Let the subscriber know about the swipe. If it is actually consumed by
      // them, it will be marked as handled.
      side_swipe_handler->OnSideSwipeBegin(side_swipe_origin, event);

      // If handled, remember the origin and then stop the further propagation
      // of the event.
      if (event->handled()) {
        current_swipe_ = side_swipe_origin;
        event->StopPropagation();
        break;
      }
    }
    return;
  }

  // Detect the end of a system gesture swipe.
  if (current_swipe_ != CastSideSwipeOrigin::NONE &&
      (event->type() == ui::ET_SCROLL_FLING_START ||
       event->type() == ui::ET_GESTURE_SCROLL_END)) {
    for (auto* side_swipe_handler : swipe_gesture_handlers_) {
      side_swipe_handler->OnSideSwipeEnd(current_swipe_, event);
    }
    current_swipe_ = CastSideSwipeOrigin::NONE;

    // Prevent this event from being used for other gesture uses.
    target->CleanupGestureState();
    event->StopPropagation();
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
