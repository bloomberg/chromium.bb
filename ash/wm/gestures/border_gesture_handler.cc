// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/border_gesture_handler.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

BorderGestureHandler::BorderGestureHandler()
    : orientation_(BORDER_SCROLL_ORIENTATION_UNSET) {
}

BorderGestureHandler::~BorderGestureHandler() {
}

bool BorderGestureHandler::ProcessGestureEvent(aura::Window* target,
                                               const ui::GestureEvent& event) {
  switch (event.type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      return HandleBorderGestureStart(target, event);
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (start_location_.any() &&
          DetermineGestureOrientation(event))
        return HandleBorderGestureUpdate(target, event);
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      return HandleBorderGestureEnd(target, event);
    default:
      break;
  }
  // If the event is outside of the display region and targetted at the root
  // window then it is on a bezel and not in another window. All bezel events
  // should be consumed here since no other part of the stack handles them.
  gfx::Rect screen =
      Shell::GetScreen()->GetDisplayNearestWindow(target).bounds();
  if (!screen.Contains(event.location()) &&
      target == target->GetRootWindow())
    return true;
  return false;
}

bool BorderGestureHandler::HandleLauncherControl(
    const ui::GestureEvent& event) {
  ShelfLayoutManager* shelf =
      Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
  switch (shelf->GetAlignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
      if (start_location_.test(BORDER_LOCATION_BOTTOM))
        return shelf_handler_.ProcessGestureEvent(event);
      break;
    case SHELF_ALIGNMENT_LEFT:
      if (start_location_.test(BORDER_LOCATION_LEFT))
        return shelf_handler_.ProcessGestureEvent(event);
      break;
    case SHELF_ALIGNMENT_TOP:
      if (start_location_.test(BORDER_LOCATION_TOP))
        return shelf_handler_.ProcessGestureEvent(event);
      break;
    case SHELF_ALIGNMENT_RIGHT:
      if (start_location_.test(BORDER_LOCATION_RIGHT))
        return shelf_handler_.ProcessGestureEvent(event);
      break;
  }
  return false;
}

bool BorderGestureHandler::HandleBorderGestureStart(
    aura::Window* target,
    const ui::GestureEvent& event) {
  orientation_ = BORDER_SCROLL_ORIENTATION_UNSET;
  start_location_.reset();

  gfx::Rect screen =
      Shell::GetScreen()->GetDisplayNearestWindow(target).bounds();
  GestureStartInTargetArea(screen, event);

  if (start_location_.any())
    return HandleLauncherControl(event);
  return false;
}

bool BorderGestureHandler::HandleBorderGestureUpdate(
    aura::Window* target,
    const ui::GestureEvent& event) {
  if (IsGestureInLauncherOrientation(event))
    return HandleLauncherControl(event);
  return false;
}

bool BorderGestureHandler::HandleBorderGestureEnd(
    aura::Window* target,
    const ui::GestureEvent& event) {
  bool ret_val = HandleLauncherControl(event);
  start_location_.reset();
  return ret_val;
}

void BorderGestureHandler::GestureStartInTargetArea(
    const gfx::Rect& screen,
    const ui::GestureEvent& event) {
  if (event.y() > screen.bottom())
    start_location_[BORDER_LOCATION_BOTTOM] = 1;
  if (event.x() < screen.x())
    start_location_[BORDER_LOCATION_LEFT] = 1;
  if (event.y() < screen.y())
    start_location_[BORDER_LOCATION_TOP] = 1;
  if (event.x() > screen.right())
    start_location_[BORDER_LOCATION_RIGHT] = 1;
}

bool BorderGestureHandler::DetermineGestureOrientation(
    const ui::GestureEvent& event) {
  if (orientation_ == BORDER_SCROLL_ORIENTATION_UNSET) {
    if (!event.details().scroll_x() && !event.details().scroll_y())
      return false;
    orientation_ = abs(event.details().scroll_y()) >
        abs(event.details().scroll_x()) ?
        BORDER_SCROLL_ORIENTATION_VERTICAL :
        BORDER_SCROLL_ORIENTATION_HORIZONTAL;
  }
  return true;
}

bool BorderGestureHandler::IsGestureInLauncherOrientation(
    const ui::GestureEvent& event) {
  BorderScrollOrientation  new_orientation = abs(event.details().scroll_y()) >
      abs(event.details().scroll_x()) ?
      BORDER_SCROLL_ORIENTATION_VERTICAL : BORDER_SCROLL_ORIENTATION_HORIZONTAL;
  if (new_orientation != orientation_)
    return false;

  ShelfLayoutManager* shelf =
      Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
  switch (shelf->GetAlignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
      if (orientation_ == BORDER_SCROLL_ORIENTATION_VERTICAL)
        return true;
      break;
    case SHELF_ALIGNMENT_LEFT:
      if (orientation_ == BORDER_SCROLL_ORIENTATION_HORIZONTAL)
        return true;
      break;
    case SHELF_ALIGNMENT_TOP:
      if (orientation_ == BORDER_SCROLL_ORIENTATION_VERTICAL)
        return true;
      break;
    case SHELF_ALIGNMENT_RIGHT:
      if (orientation_ == BORDER_SCROLL_ORIENTATION_HORIZONTAL)
        return true;
      break;
  }
  return false;
}

}  // namespace internal
}  // namespace ash
