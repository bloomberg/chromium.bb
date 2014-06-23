// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/event_transformation_handler.h"

#include <cmath>

#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/dip_util.h"
#include "ui/events/event.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

#if defined(OS_CHROMEOS)
#include "ui/display/chromeos/display_configurator.h"
#endif  // defined(OS_CHROMEOS)

namespace ash {
namespace {

// Boost factor for non-integrated displays.
const float kBoostForNonIntegrated = 1.20f;

#if defined(OS_CHROMEOS)
gfx::Size GetNativeModeSize(const DisplayInfo& info) {
  const std::vector<DisplayMode>& modes = info.display_modes();
  for (size_t i = 0; i < modes.size(); ++i) {
    if (modes[i].native)
      return modes[i].size;
  }

  return gfx::Size();
}

float GetMirroredDisplayAreaRatio(const gfx::Size& current_size,
                                  const gfx::Size& native_size) {
  float area_ratio = 1.0f;

  if (current_size.IsEmpty() || native_size.IsEmpty())
    return area_ratio;

  float width_ratio = static_cast<float>(current_size.width()) /
                      static_cast<float>(native_size.width());
  float height_ratio = static_cast<float>(current_size.height()) /
                       static_cast<float>(native_size.height());

  area_ratio = width_ratio * height_ratio;
  return area_ratio;
}

// Key of the map is the touch display's id, and the value of the map is the
// touch display's area ratio in mirror mode defined as:
// mirror_mode_area / native_mode_area.
// This is used for scaling touch event's radius when the touch display is in
// mirror mode: new_touch_radius = sqrt(area_ratio) * old_touch_radius
std::map<int, float> GetMirroredDisplayAreaRatioMap() {
  std::map<int, float> area_ratios;
  DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  const std::vector<gfx::Display>& displays = display_manager->displays();
  for (size_t i = 0; i < displays.size(); ++i) {
    const DisplayInfo& info = display_manager->GetDisplayInfo(
        displays[i].id());
    const gfx::Size current_size = info.size_in_pixel();
    const gfx::Size native_size = GetNativeModeSize(info);

    if (info.touch_support() == gfx::Display::TOUCH_SUPPORT_AVAILABLE &&
        current_size != native_size &&
        info.is_aspect_preserving_scaling()) {
      area_ratios[info.touch_device_id()] = GetMirroredDisplayAreaRatio(
          current_size, native_size);
    }
  }

  return area_ratios;
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

EventTransformationHandler::EventTransformationHandler()
    : transformation_mode_(TRANSFORM_AUTO) {
}

EventTransformationHandler::~EventTransformationHandler() {
}

void EventTransformationHandler::OnScrollEvent(ui::ScrollEvent* event) {
  if (transformation_mode_ == TRANSFORM_NONE)
    return;

  // It is unnecessary to scale the event for the device scale factor since
  // the event locations etc. are already in DIP.
  gfx::Point point_in_screen(event->location());
  aura::Window* target = static_cast<aura::Window*>(event->target());
  wm::ConvertPointToScreen(target, &point_in_screen);
  const gfx::Display& display =
      Shell::GetScreen()->GetDisplayNearestPoint(point_in_screen);

  // Apply some additional scaling if the display is non-integrated.
  if (!display.IsInternal())
    event->Scale(kBoostForNonIntegrated);
}

#if defined(OS_CHROMEOS)
// This is to scale the TouchEvent's radius when the touch display is in
// mirror mode. TouchEvent's radius is often reported in the touchscreen's
// native resolution. In mirror mode, the touch display could be configured
// at a lower resolution. We scale down the radius using the ratio defined as
// the sqrt of
// (mirror_width * mirror_height) / (native_width * native_height)
void EventTransformationHandler::OnTouchEvent(ui::TouchEvent* event) {
  // Check display_configurator's output_state instead of checking
  // DisplayManager::IsMirrored() because the compositor based mirroring
  // won't cause the scaling issue.
  if (ash::Shell::GetInstance()->display_configurator()->display_state() !=
      ui::MULTIPLE_DISPLAY_STATE_DUAL_MIRROR)
    return;

  const std::map<int, float> area_ratio_map = GetMirroredDisplayAreaRatioMap();

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

}  // namespace ash
