// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/event_transformation_handler.h"

#include <cmath>

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

#if defined(OS_CHROMEOS)
#include "chromeos/display/output_configurator.h"
#endif  // defined(OS_CHROMEOS)

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
  if (!display.IsInternal())
    scale *= kBoostForNonIntegrated;

  event->Scale(scale);
}

#if defined(OS_CHROMEOS)
// This is to scale the TouchEvent's radius when the touch display is in
// mirror mode. TouchEvent's radius is often reported in the touchscreen's
// native resolution. In mirror mode, the touch display could be configured
// at a lower resolution. We scale down the radius using the ratio defined as
// the sqrt of
// (mirror_width * mirror_height) / (native_width * native_height)
void EventTransformationHandler::OnTouchEvent(ui::TouchEvent* event) {
  using chromeos::OutputConfigurator;
  OutputConfigurator* output_configurator =
      ash::Shell::GetInstance()->output_configurator();

  if (output_configurator->output_state() != chromeos::STATE_DUAL_MIRROR)
    return;

  const std::map<int, float>& area_ratio_map =
      output_configurator->GetMirroredDisplayAreaRatioMap();

  // TODO(miletus): When there are more than 1 touchscreen (e.g. Link connected
  // to an external touchscreen), the correct way to do is to have a way
  // to find out which touchscreen is the event originating from and use the
  // area ratio of that touchscreen to scale the event's radius.
  // Tracked here crbug.com/233245
  if (area_ratio_map.size() != 1) {
    LOG(ERROR) << "Mirroring mode with " << area_ratio_map.size()
               << " touch display found";
    return;
  }

  float area_ratio_sqrt = std::sqrt(area_ratio_map.begin()->second);
  event->set_radius_x(event->radius_x() * area_ratio_sqrt);
  event->set_radius_y(event->radius_y() * area_ratio_sqrt);
}
#endif  // defined(OS_CHROMEOS)

}  // namespace internal
}  // namespace ash
