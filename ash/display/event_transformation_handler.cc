// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/event_transformation_handler.h"

#include "ash/display/display_manager.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_util.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {
namespace {

// Boost factor for non-integrated displays.
const float kBoostForNonIntegrated = 1.20f;
}

EventTransformationHandler::EventTransformationHandler()
    : transformation_mode_(TRANSFORM_AUTO) {
}

EventTransformationHandler::~EventTransformationHandler() {
}

void EventTransformationHandler::OnScrollEvent(ui::ScrollEvent* event) {
  if (transformation_mode_ == TRANSFORM_NONE)
    return;

  // Get the device scale factor and stack it on the final scale factor.
  gfx::Point point_in_screen(event->location());
  aura::Window* target = static_cast<aura::Window*>(event->target());
  const float scale_at_target = ui::GetDeviceScaleFactor(target->layer());
  float scale = scale_at_target;

  // Apply some additional scaling if the display is non-integrated.
  wm::ConvertPointToScreen(target, &point_in_screen);
  const gfx::Display& display =
      Shell::GetScreen()->GetDisplayNearestPoint(point_in_screen);
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  if (!display_manager->IsInternalDisplayId(display.id()))
    scale *= kBoostForNonIntegrated;

  event->Scale(scale);
}

}  // namespace internal
}  // namespace ash

