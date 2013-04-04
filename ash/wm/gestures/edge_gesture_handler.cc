// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/edge_gesture_handler.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ui/base/events/event.h"
#include "ui/gfx/screen.h"

namespace {
const int kBottomEdgeGestureThreshold = 10;
const int kLeftEdgeGestureThreshold = 10;
const int kRightEdgeGestureThreshold = 10;
const int kTopEdgeGestureThreshold = 10;

// Bit positions of edge start flags
enum EdgeStart {
  EDGE_START_TOP = 0,
  EDGE_START_LEFT = 1,
  EDGE_START_RIGHT = 2,
  EDGE_START_BOTTOM = 3
};

// Helpers for manipulating edge start flags
#define CLEAR_FLAGS(FLAGS) (FLAGS = 0)
#define SET_FLAG(FLAGS, FLAG_POS) (FLAGS |= 1 << FLAG_POS)
#define IS_FLAG_SET(FLAGS, FLAG_POS) (0 != (FLAGS & (1 << FLAG_POS)))
#define ANY_FLAGS_SET(FLAGS) (FLAGS != 0)
}  // namespace

namespace ash {
namespace internal {

EdgeGestureHandler::EdgeGestureHandler() :
    orientation_(EDGE_SCROLL_ORIENTATION_UNSET) {
  CLEAR_FLAGS(start_location_);
}

EdgeGestureHandler::~EdgeGestureHandler() {
}

bool EdgeGestureHandler::ProcessGestureEvent(aura::Window* target,
                                             const ui::GestureEvent& event) {
  // TODO(crbug.com/222746): Rewrite this code to ignore events that are for
  // edges that a bezel is present for instead of hardcoding the layout of edge
  // v bezel.
  switch (event.type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      return HandleEdgeGestureStart(target, event);
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (ANY_FLAGS_SET(start_location_)) {
        if (DetermineGestureOrientation(event)) {
          return HandleEdgeGestureUpdate(target, event);
        }
      }
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      return HandleEdgeGestureEnd(target, event);
    default:
      break;
  }
  return false;
}

bool EdgeGestureHandler::HandleLauncherControl(const ui::GestureEvent& event) {
  ShelfLayoutManager* shelf =
      Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
  // If visible then let the shelf handle it as a normal widget.
  if (shelf->IsVisible())
    return false;

  switch (shelf->GetAlignment()) {
    case SHELF_ALIGNMENT_LEFT:
      if (IS_FLAG_SET(start_location_, EDGE_START_LEFT))
        return shelf_handler_.ProcessGestureEvent(event);
      break;
    case SHELF_ALIGNMENT_RIGHT:
      if (IS_FLAG_SET(start_location_, EDGE_START_RIGHT))
        return shelf_handler_.ProcessGestureEvent(event);
      break;
    // Let the BezelGestureHandler handle gestures on the bottom edge of the
    // screen and there are no implemented gestures for the top edge at the
    // moment.
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_TOP:
    default:
      break;
  }
  return false;
}

bool EdgeGestureHandler::HandleEdgeGestureStart(
    aura::Window* target,
    const ui::GestureEvent& event) {
  gfx::Rect screen =
      Shell::GetScreen()->GetDisplayNearestWindow(target).bounds();

  orientation_ = EDGE_SCROLL_ORIENTATION_UNSET;
  if (GestureStartInEdgeArea(screen, event))
    return HandleLauncherControl(event);
  return false;
}

bool EdgeGestureHandler::DetermineGestureOrientation(
    const ui::GestureEvent& event) {
  if (orientation_ == EDGE_SCROLL_ORIENTATION_UNSET) {
    if (!event.details().scroll_x() && !event.details().scroll_y())
      return false;
    orientation_ = abs(event.details().scroll_y()) >
        abs(event.details().scroll_x()) ?
        EDGE_SCROLL_ORIENTATION_VERTICAL : EDGE_SCROLL_ORIENTATION_HORIZONTAL;
  }
  return true;
}

bool EdgeGestureHandler::HandleEdgeGestureUpdate(
    aura::Window* target,
    const ui::GestureEvent& event) {
  if (IsGestureInLauncherOrientation(event))
    return HandleLauncherControl(event);
  return false;
}

bool EdgeGestureHandler::HandleEdgeGestureEnd(aura::Window* target,
                                              const ui::GestureEvent& event) {
  bool ret_val = HandleLauncherControl(event);
  CLEAR_FLAGS(start_location_);
  return ret_val;
}

bool EdgeGestureHandler::GestureStartInEdgeArea(
    const gfx::Rect& screen,
    const ui::GestureEvent& event) {
  CLEAR_FLAGS(start_location_);

  if (abs(event.y() - screen.y()) < kTopEdgeGestureThreshold &&
      event.y() >= 0)
    SET_FLAG(start_location_, EDGE_START_TOP);
  if (abs(event.x() - screen.x()) < kLeftEdgeGestureThreshold &&
      event.x() >= 0)
    SET_FLAG(start_location_, EDGE_START_LEFT);
  if (abs(event.x() - screen.right()) < kRightEdgeGestureThreshold &&
      event.x() >= 0)
    SET_FLAG(start_location_, EDGE_START_RIGHT);
  if (abs(event.y() - screen.bottom()) < kBottomEdgeGestureThreshold &&
      event.y() >= 0)
    SET_FLAG(start_location_, EDGE_START_BOTTOM);
  return ANY_FLAGS_SET(start_location_);
}

bool EdgeGestureHandler::IsGestureInLauncherOrientation(
    const ui::GestureEvent& event) {
  EdgeScrollOrientation  new_orientation = abs(event.details().scroll_y()) >
      abs(event.details().scroll_x()) ?
      EDGE_SCROLL_ORIENTATION_VERTICAL : EDGE_SCROLL_ORIENTATION_HORIZONTAL;

  if (new_orientation != orientation_)
    return false;

  ShelfLayoutManager* shelf =
      Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
  switch (shelf->GetAlignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
      if (orientation_ == EDGE_SCROLL_ORIENTATION_VERTICAL)
        return true;
      break;
    case SHELF_ALIGNMENT_LEFT:
      if (orientation_ == EDGE_SCROLL_ORIENTATION_HORIZONTAL)
        return true;
      break;
    case SHELF_ALIGNMENT_RIGHT:
      if (orientation_ == EDGE_SCROLL_ORIENTATION_HORIZONTAL)
        return true;
      break;
    case SHELF_ALIGNMENT_TOP:
      if (orientation_ == EDGE_SCROLL_ORIENTATION_VERTICAL)
        return true;
      break;
    default:
      break;
  }
  return false;
}

}  // namespace internal
}  // namespace ash
