// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/display_info_provider_chromeos.h"

#include <stdint.h>

#include "ash/display/display_configuration_controller.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/shell.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chrome/browser/chromeos/display/overscan_calibrator.h"
#include "chrome/browser/chromeos/display/touch_calibrator/touch_calibrator_controller.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "extensions/common/api/system_display.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {

namespace system_display = api::system_display;

namespace {

// Maximum allowed bounds origin absolute value.
const int kMaxBoundsOrigin = 200 * 1000;

// Gets the display with the provided string id.
display::Display GetDisplay(const std::string& display_id_str) {
  int64_t display_id;
  if (!base::StringToInt64(display_id_str, &display_id))
    return display::Display();
  return ash::Shell::Get()->display_manager()->GetDisplayForId(display_id);
}

// Checks if the given integer value is valid display rotation in degrees.
bool IsValidRotationValue(int rotation) {
  return rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270;
}

// Converts integer integer value in degrees to Rotation enum value.
display::Display::Rotation DegreesToRotation(int degrees) {
  DCHECK(IsValidRotationValue(degrees));
  switch (degrees) {
    case 0:
      return display::Display::ROTATE_0;
    case 90:
      return display::Display::ROTATE_90;
    case 180:
      return display::Display::ROTATE_180;
    case 270:
      return display::Display::ROTATE_270;
    default:
      return display::Display::ROTATE_0;
  }
}

// Converts system_display::LayoutPosition to
// display::DisplayPlacement::Position.
display::DisplayPlacement::Position GetDisplayPlacementPosition(
    system_display::LayoutPosition position) {
  switch (position) {
    case system_display::LAYOUT_POSITION_TOP:
      return display::DisplayPlacement::TOP;
    case system_display::LAYOUT_POSITION_BOTTOM:
      return display::DisplayPlacement::BOTTOM;
    case system_display::LAYOUT_POSITION_LEFT:
      return display::DisplayPlacement::LEFT;
    case system_display::LAYOUT_POSITION_RIGHT:
    default:
      // Default: layout to the right.
      return display::DisplayPlacement::RIGHT;
  }
}

// Converts display::DisplayPlacement::Position to
// system_display::LayoutPosition.
system_display::LayoutPosition GetLayoutPosition(
    display::DisplayPlacement::Position position) {
  switch (position) {
    case display::DisplayPlacement::TOP:
      return system_display::LayoutPosition::LAYOUT_POSITION_TOP;
    case display::DisplayPlacement::RIGHT:
      return system_display::LayoutPosition::LAYOUT_POSITION_RIGHT;
    case display::DisplayPlacement::BOTTOM:
      return system_display::LayoutPosition::LAYOUT_POSITION_BOTTOM;
    case display::DisplayPlacement::LEFT:
      return system_display::LayoutPosition::LAYOUT_POSITION_LEFT;
  }
  return system_display::LayoutPosition::LAYOUT_POSITION_NONE;
}

// Checks if the given point is over the radius vector described by it's end
// point |vector|. The point is over a vector if it's on its positive (left)
// side. The method sees a point on the same line as the vector as being over
// the vector.
bool PointIsOverRadiusVector(const gfx::Point& point,
                             const gfx::Point& vector) {
  // |point| is left of |vector| if its radius vector's scalar product with a
  // vector orthogonal (and facing the positive side) to |vector| is positive.
  //
  // An orthogonal vector of (a, b) is (b, -a), as the scalar product of these
  // two is 0.
  // So, (x, y) is over (a, b) if x * b + y * (-a) >= 0, which is equivalent to
  // x * b >= y * a.
  return static_cast<int64_t>(point.x()) * static_cast<int64_t>(vector.y()) >=
         static_cast<int64_t>(point.y()) * static_cast<int64_t>(vector.x());
}

// Created display::DisplayPlacement value for |rectangle| compared to the
// |reference|
// rectangle.
// The layout consists of two values:
//   - position: Whether the rectangle is positioned left, right, over or under
//     the reference.
//   - offset: The rectangle's offset from the reference origin along the axis
//     opposite the position direction (if the rectangle is left or right along
//     y-axis, otherwise along x-axis).
// The rectangle's position is calculated by dividing the space in areas defined
// by the |reference|'s diagonals and finding the area |rectangle|'s center
// point belongs. If the |rectangle| in the calculated layout does not share a
// part of the bounds with the |reference|, the |rectangle| position in set to
// the more suitable neighboring position (e.g. if |rectangle| is completely
// over the |reference| top bound, it will be set to TOP) and the layout is
// recalculated with the new position. This is to handle case where the
// rectangle shares an edge with the reference, but it's center is not in the
// same area as the reference's edge, e.g.
//
// +---------------------+
// |                     |
// | REFERENCE           |
// |                     |
// |                     |
// +---------------------+
//                 +-------------------------------------------------+
//                 | RECTANGLE               x                       |
//                 +-------------------------------------------------+
//
// The rectangle shares an egde with the reference's bottom edge, but it's
// center point is in the left area.
display::DisplayPlacement CreatePlacementForRectangles(
    const gfx::Rect& reference,
    const gfx::Rect& rectangle) {
  // Translate coordinate system so origin is in the reference's top left point
  // (so the reference's down-diagonal vector starts in the (0, 0)) and scale it
  // up by two (to avoid division when calculating the rectangle's center
  // point).
  gfx::Point center(2 * (rectangle.x() - reference.x()) + rectangle.width(),
                    2 * (rectangle.y() - reference.y()) + rectangle.height());
  gfx::Point down_diag(2 * reference.width(), 2 * reference.height());

  bool is_top_right = PointIsOverRadiusVector(center, down_diag);

  // Translate the coordinating system again, so the bottom right point of the
  // reference is origin (so the references up-diagonal starts at (0, 0)).
  // Note that the coordinate system is scaled by 2.
  center.Offset(0, -2 * reference.height());
  // Choose the vector orientation so the points on the diagonal are considered
  // to be left.
  gfx::Point up_diag(-2 * reference.width(), 2 * reference.height());

  bool is_bottom_right = PointIsOverRadiusVector(center, up_diag);

  display::DisplayPlacement::Position position;
  if (is_top_right) {
    position = is_bottom_right ? display::DisplayPlacement::RIGHT
                               : display::DisplayPlacement::TOP;
  } else {
    position = is_bottom_right ? display::DisplayPlacement::BOTTOM
                               : display::DisplayPlacement::LEFT;
  }

  // If the rectangle with the calculated position would not have common side
  // with the reference, try to position it so it shares another edge with the
  // reference.
  if (is_top_right == is_bottom_right) {
    if (rectangle.y() > reference.y() + reference.height()) {
      // The rectangle is left or right, but completely under the reference.
      position = display::DisplayPlacement::BOTTOM;
    } else if (rectangle.y() + rectangle.height() < reference.y()) {
      // The rectangle is left or right, but completely over the reference.
      position = display::DisplayPlacement::TOP;
    }
  } else {
    if (rectangle.x() > reference.x() + reference.width()) {
      // The rectangle is over or under, but completely right of the reference.
      position = display::DisplayPlacement::RIGHT;
    } else if (rectangle.x() + rectangle.width() < reference.x()) {
      // The rectangle is over or under, but completely left of the reference.
      position = display::DisplayPlacement::LEFT;
    }
  }
  int offset = (position == display::DisplayPlacement::LEFT ||
                position == display::DisplayPlacement::RIGHT)
                   ? rectangle.y()
                   : rectangle.x();
  return display::DisplayPlacement(position, offset);
}

// Updates the display layout for the target display in reference to the primary
// display.
void UpdateDisplayLayout(const gfx::Rect& primary_display_bounds,
                         int64_t primary_display_id,
                         const gfx::Rect& target_display_bounds,
                         int64_t target_display_id) {
  display::DisplayPlacement placement(CreatePlacementForRectangles(
      primary_display_bounds, target_display_bounds));
  placement.display_id = target_display_id;
  placement.parent_display_id = primary_display_id;

  std::unique_ptr<display::DisplayLayout> layout(new display::DisplayLayout);
  layout->placement_list.push_back(placement);
  layout->primary_id = primary_display_id;

  ash::Shell::Get()->display_configuration_controller()->SetDisplayLayout(
      std::move(layout));
}

// Validates that parameters passed to the SetInfo function are valid for the
// desired display and the current display manager state.
// Returns whether the parameters are valid. On failure |error| is set to the
// error message.
bool ValidateParamsForDisplay(const system_display::DisplayProperties& info,
                              const display::Display& display,
                              display::DisplayManager* display_manager,
                              int64_t primary_display_id,
                              std::string* error) {
  int64_t id = display.id();
  bool is_primary =
      id == primary_display_id || (info.is_primary && *info.is_primary);

  if (info.is_unified) {
    if (!is_primary) {
      *error = "Unified desktop mode can only be set for the primary display.";
      return false;
    }
    if (info.mirroring_source_id) {
      *error = "Unified desktop mode can not be set with mirroringSourceId.";
      return false;
    }
    return true;
  }

  // If mirroring source id is set, a display with the given id should exist,
  // and if should not be the same as the target display's id.
  if (info.mirroring_source_id && !info.mirroring_source_id->empty()) {
    int64_t mirroring_id = GetDisplay(*info.mirroring_source_id).id();
    if (mirroring_id == display::kInvalidDisplayId) {
      *error = "Display " + *info.mirroring_source_id + " not found.";
      return false;
    }

    if (*info.mirroring_source_id == base::Int64ToString(id)) {
      *error = "Not allowed to mirror self.";
      return false;
    }
  }

  // If mirroring source parameter is specified, no other parameter should be
  // set as when the mirroring is applied the display list could change.
  if (info.mirroring_source_id &&
      (info.is_primary || info.bounds_origin_x || info.bounds_origin_y ||
       info.rotation || info.overscan)) {
    *error = "No other parameter should be set alongside mirroringSourceId.";
    return false;
  }

  // The bounds cannot be changed for the primary display and should be inside
  // a reasonable bounds. Note that the display is considered primary if the
  // info has 'isPrimary' parameter set, as this will be applied before bounds
  // origin changes.
  if (info.bounds_origin_x || info.bounds_origin_y) {
    if (is_primary) {
      *error = "Bounds origin not allowed for the primary display.";
      return false;
    }
    if (info.bounds_origin_x && (*info.bounds_origin_x > kMaxBoundsOrigin ||
                                 *info.bounds_origin_x < -kMaxBoundsOrigin)) {
      *error = "Bounds origin x out of bounds.";
      return false;
    }
    if (info.bounds_origin_y && (*info.bounds_origin_y > kMaxBoundsOrigin ||
                                 *info.bounds_origin_y < -kMaxBoundsOrigin)) {
      *error = "Bounds origin y out of bounds.";
      return false;
    }
  }

  // Verify the rotation value is valid.
  if (info.rotation && !IsValidRotationValue(*info.rotation)) {
    *error = "Invalid rotation.";
    return false;
  }

  // Overscan cannot be changed for the internal display, and should be at most
  // half of the screen size.
  if (info.overscan) {
    if (display.IsInternal()) {
      *error = "Overscan changes not allowed for the internal monitor.";
      return false;
    }

    if (info.overscan->left < 0 || info.overscan->top < 0 ||
        info.overscan->right < 0 || info.overscan->bottom < 0) {
      *error = "Negative overscan not allowed.";
      return false;
    }

    const gfx::Insets overscan = display_manager->GetOverscanInsets(id);
    int screen_width = display.bounds().width() + overscan.width();
    int screen_height = display.bounds().height() + overscan.height();

    if ((info.overscan->left + info.overscan->right) * 2 > screen_width) {
      *error = "Horizontal overscan is more than half of the screen width.";
      return false;
    }

    if ((info.overscan->top + info.overscan->bottom) * 2 > screen_height) {
      *error = "Vertical overscan is more than half of the screen height.";
      return false;
    }
  }

  // Set the display mode.
  if (info.display_mode) {
    scoped_refptr<display::ManagedDisplayMode> current_mode =
        display_manager->GetActiveModeForDisplayId(id);
    // Copy properties not set in the UI from the current mode.
    gfx::Size size(info.display_mode->width_in_native_pixels,
                   info.display_mode->height_in_native_pixels);

    // NB: info.display_mode is neither a display::ManagedDisplayMode or a
    // display::DisplayMode.
    scoped_refptr<display::ManagedDisplayMode> new_mode(
        new display::ManagedDisplayMode(
            size, current_mode->refresh_rate(), current_mode->is_interlaced(),
            info.display_mode->is_native, info.display_mode->ui_scale,
            info.display_mode->device_scale_factor));

    if (new_mode->IsEquivalent(current_mode)) {
      *error = "Display mode matches current mode.";
      return false;
    }

    // If it's the internal display, the display mode will be applied directly,
    // otherwise a confirm/revert notification will be prepared first, and the
    // display mode will be applied. If the user accepts the mode change by
    // dismissing the notification, StoreDisplayPrefs() will be called back to
    // persist the new preferences.
    if (!ash::Shell::Get()
             ->resolution_notification_controller()
             ->PrepareNotificationAndSetDisplayMode(
                 id, current_mode, new_mode,
                 base::Bind(&chromeos::StoreDisplayPrefs))) {
      *error = "Unable to set the display mode.";
      return false;
    }
  }
  return true;
}

system_display::DisplayMode GetDisplayMode(
    display::DisplayManager* display_manager,
    const display::ManagedDisplayInfo& display_info,
    const scoped_refptr<display::ManagedDisplayMode>& display_mode) {
  system_display::DisplayMode result;

  bool is_internal = display::Display::HasInternalDisplay() &&
                     display::Display::InternalDisplayId() == display_info.id();
  gfx::Size size_dip = display_mode->GetSizeInDIP(is_internal);
  result.width = size_dip.width();
  result.height = size_dip.height();
  result.width_in_native_pixels = display_mode->size().width();
  result.height_in_native_pixels = display_mode->size().height();
  result.ui_scale = display_mode->ui_scale();
  result.device_scale_factor = display_mode->device_scale_factor();
  result.is_native = display_mode->native();
  result.is_selected = display_mode->IsEquivalent(
      display_manager->GetActiveModeForDisplayId(display_info.id()));
  return result;
}

display::TouchCalibrationData::CalibrationPointPair GetCalibrationPair(
    const system_display::TouchCalibrationPair& pair) {
  return std::make_pair(gfx::Point(pair.display_point.x, pair.display_point.y),
                        gfx::Point(pair.touch_point.x, pair.touch_point.y));
}

bool ValidateParamsForTouchCalibration(
    const std::string& id,
    const display::Display& display,
    chromeos::TouchCalibratorController* const touch_calibrator_controller,
    std::string* error) {
  if (display.id() == display::kInvalidDisplayId) {
    *error = "Display Id(" + id + ") is an invalid display ID";
    return false;
  }

  if (display.IsInternal()) {
    *error = "Display Id(" + id + ") is an internal display. Internal " +
             "displays cannot be calibrated for touch.";
    return false;
  }

  if (display.touch_support() != display::Display::TOUCH_SUPPORT_AVAILABLE) {
    *error = "Display Id(" + id + ") does not support touch.";
    return false;
  }

  return true;
}

bool IsTabletModeWindowManagerEnabled() {
  return ash::Shell::Get()
      ->tablet_mode_controller()
      ->IsTabletModeWindowManagerEnabled();
}

}  // namespace

// static
const char
    DisplayInfoProviderChromeOS::kCustomTouchCalibrationInProgressError[] =
        "Another custom touch calibration already under progress.";

// static
const char
    DisplayInfoProviderChromeOS::kCompleteCalibrationCalledBeforeStartError[] =
        "system.display.completeCustomTouchCalibration called before "
        "system.display.startCustomTouchCalibration before.";

// static
const char DisplayInfoProviderChromeOS::kTouchBoundsNegativeError[] =
    "Bounds cannot have negative values.";

// static
const char DisplayInfoProviderChromeOS::kTouchCalibrationPointsNegativeError[] =
    "Display points and touch points cannot have negative coordinates";

// static
const char DisplayInfoProviderChromeOS::kTouchCalibrationPointsTooLargeError[] =
    "Display point coordinates cannot be more than size of the display.";

// static
const char DisplayInfoProviderChromeOS::kNativeTouchCalibrationActiveError[] =
    "Another touch calibration is already active.";

DisplayInfoProviderChromeOS::DisplayInfoProviderChromeOS() {}

DisplayInfoProviderChromeOS::~DisplayInfoProviderChromeOS() {}

bool DisplayInfoProviderChromeOS::SetInfo(
    const std::string& display_id_str,
    const system_display::DisplayProperties& info,
    std::string* error) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    *error = "Not implemented for mash.";
    return false;
  }

  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  ash::DisplayConfigurationController* display_configuration_controller =
      ash::Shell::Get()->display_configuration_controller();

  const display::Display target = GetDisplay(display_id_str);

  if (target.id() == display::kInvalidDisplayId) {
    *error = "Display not found:" + display_id_str;
    return false;
  }

  int64_t display_id = target.id();
  const display::Display& primary =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  if (!ValidateParamsForDisplay(info, target, display_manager, primary.id(),
                                error)) {
    return false;
  }

  // Process 'isUnified' parameter if set.
  if (info.is_unified) {
    display_manager->SetDefaultMultiDisplayModeForCurrentDisplays(
        *info.is_unified ? display::DisplayManager::UNIFIED
                         : display::DisplayManager::EXTENDED);
  }

  // Process 'isPrimary' parameter.
  if (info.is_primary && *info.is_primary && target.id() != primary.id())
    display_configuration_controller->SetPrimaryDisplayId(display_id);

  // Process 'mirroringSourceId' parameter.
  if (info.mirroring_source_id) {
    bool mirror = !info.mirroring_source_id->empty();
    display_configuration_controller->SetMirrorMode(mirror);
  }

  // Process 'overscan' parameter.
  if (info.overscan) {
    display_manager->SetOverscanInsets(
        display_id, gfx::Insets(info.overscan->top, info.overscan->left,
                                info.overscan->bottom, info.overscan->right));
  }

  // Process 'rotation' parameter.
  if (info.rotation) {
    if (IsTabletModeWindowManagerEnabled() &&
        display_id == display::Display::InternalDisplayId()) {
      ash::Shell::Get()->screen_orientation_controller()->SetLockToRotation(
          DegreesToRotation(*info.rotation));
    } else {
      display_configuration_controller->SetDisplayRotation(
          display_id, DegreesToRotation(*info.rotation),
          display::Display::ROTATION_SOURCE_USER);
    }
  }

  // Process new display origin parameters.
  gfx::Point new_bounds_origin = target.bounds().origin();
  if (info.bounds_origin_x)
    new_bounds_origin.set_x(*info.bounds_origin_x);
  if (info.bounds_origin_y)
    new_bounds_origin.set_y(*info.bounds_origin_y);

  if (new_bounds_origin != target.bounds().origin()) {
    gfx::Rect target_bounds = target.bounds();
    target_bounds.Offset(new_bounds_origin.x() - target.bounds().x(),
                         new_bounds_origin.y() - target.bounds().y());
    UpdateDisplayLayout(primary.bounds(), primary.id(), target_bounds,
                        target.id());
  }

  return true;
}

bool DisplayInfoProviderChromeOS::SetDisplayLayout(
    const DisplayLayoutList& layouts) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    return false;
  }
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  display::DisplayLayoutBuilder builder(
      display_manager->GetCurrentResolvedDisplayLayout());

  bool have_root = false;
  builder.ClearPlacements();
  for (const system_display::DisplayLayout& layout : layouts) {
    display::Display display = GetDisplay(layout.id);
    if (display.id() == display::kInvalidDisplayId) {
      LOG(ERROR) << "Invalid layout: display id not found: " << layout.id;
      return false;
    }
    display::Display parent = GetDisplay(layout.parent_id);
    if (parent.id() == display::kInvalidDisplayId) {
      if (have_root) {
        LOG(ERROR) << "Invalid layout: multople roots.";
        return false;
      }
      have_root = true;
      continue;  // No placement for root (primary) display.
    }
    display::DisplayPlacement::Position position =
        GetDisplayPlacementPosition(layout.position);
    builder.AddDisplayPlacement(display.id(), parent.id(), position,
                                layout.offset);
  }
  std::unique_ptr<display::DisplayLayout> layout = builder.Build();
  if (!display::DisplayLayout::Validate(
          display_manager->GetCurrentDisplayIdList(), *layout)) {
    LOG(ERROR) << "Invalid layout: Validate failed.";
    return false;
  }
  ash::Shell::Get()->display_configuration_controller()->SetDisplayLayout(
      std::move(layout));
  return true;
}

void DisplayInfoProviderChromeOS::UpdateDisplayUnitInfoForPlatform(
    const display::Display& display,
    system_display::DisplayUnitInfo* unit) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    return;
  }
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  unit->name = display_manager->GetDisplayNameForId(display.id());
  if (display_manager->IsInMirrorMode()) {
    unit->mirroring_source_id =
        base::Int64ToString(display_manager->mirroring_display_id());
  }

  const display::ManagedDisplayInfo& display_info =
      display_manager->GetDisplayInfo(display.id());
  const float device_dpi = display_info.device_dpi();
  unit->dpi_x = device_dpi * display.size().width() /
                display_info.bounds_in_native().width();
  unit->dpi_y = device_dpi * display.size().height() /
                display_info.bounds_in_native().height();

  const gfx::Insets overscan_insets =
      display_manager->GetOverscanInsets(display.id());
  unit->overscan.left = overscan_insets.left();
  unit->overscan.top = overscan_insets.top();
  unit->overscan.right = overscan_insets.right();
  unit->overscan.bottom = overscan_insets.bottom();

  for (const scoped_refptr<display::ManagedDisplayMode>& display_mode :
       display_info.display_modes()) {
    unit->modes.push_back(
        GetDisplayMode(display_manager, display_info, display_mode));
  }
}

void DisplayInfoProviderChromeOS::EnableUnifiedDesktop(bool enable) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    return;
  }
  ash::Shell::Get()->display_manager()->SetUnifiedDesktopEnabled(enable);
}

DisplayInfoProvider::DisplayUnitInfoList
DisplayInfoProviderChromeOS::GetAllDisplaysInfo(bool single_unified) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    return DisplayInfoProvider::DisplayUnitInfoList();
  }
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();

  if (!display_manager->IsInUnifiedMode())
    return DisplayInfoProvider::GetAllDisplaysInfo(single_unified);

  // Chrome OS specific: get displays for unified mode.
  std::vector<display::Display> displays;
  int64_t primary_id;
  if (single_unified) {
    for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i)
      displays.push_back(display_manager->GetDisplayAt(i));
    primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  } else {
    displays = display_manager->software_mirroring_display_list();
    CHECK_GT(displays.size(), 0u);
    // Use first display as primary.
    primary_id = displays[0].id();
  }

  DisplayUnitInfoList all_displays;
  for (const display::Display& display : displays) {
    system_display::DisplayUnitInfo unit_info =
        CreateDisplayUnitInfo(display, primary_id);
    UpdateDisplayUnitInfoForPlatform(display, &unit_info);
    unit_info.is_unified = true;
    all_displays.push_back(std::move(unit_info));
  }
  return all_displays;
}

DisplayInfoProvider::DisplayLayoutList
DisplayInfoProviderChromeOS::GetDisplayLayout() {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    return DisplayInfoProvider::DisplayLayoutList();
  }
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();

  if (display_manager->num_connected_displays() < 2)
    return DisplayInfoProvider::DisplayLayoutList();

  display::Screen* screen = display::Screen::GetScreen();
  std::vector<display::Display> displays = screen->GetAllDisplays();

  DisplayLayoutList result;
  for (const display::Display& display : displays) {
    const display::DisplayPlacement placement =
        display_manager->GetCurrentResolvedDisplayLayout().FindPlacementById(
            display.id());
    if (placement.display_id == display::kInvalidDisplayId)
      continue;
    system_display::DisplayLayout display_layout;
    display_layout.id = base::Int64ToString(placement.display_id);
    display_layout.parent_id = base::Int64ToString(placement.parent_display_id);
    display_layout.position = GetLayoutPosition(placement.position);
    display_layout.offset = placement.offset;
    result.push_back(std::move(display_layout));
  }
  return result;
}

bool DisplayInfoProviderChromeOS::OverscanCalibrationStart(
    const std::string& id) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    return false;
  }
  VLOG(1) << "OverscanCalibrationStart: " << id;
  const display::Display display = GetDisplay(id);
  if (display.id() == display::kInvalidDisplayId)
    return false;
  auto insets =
      ash::Shell::Get()->window_tree_host_manager()->GetOverscanInsets(
          display.id());
  overscan_calibrators_[id].reset(
      new chromeos::OverscanCalibrator(display, insets));
  return true;
}

bool DisplayInfoProviderChromeOS::OverscanCalibrationAdjust(
    const std::string& id,
    const system_display::Insets& delta) {
  VLOG(1) << "OverscanCalibrationAdjust: " << id;
  chromeos::OverscanCalibrator* calibrator = GetOverscanCalibrator(id);
  if (!calibrator)
    return false;
  gfx::Insets insets = calibrator->insets();
  insets += gfx::Insets(delta.top, delta.left, delta.bottom, delta.right);
  calibrator->UpdateInsets(insets);
  return true;
}

bool DisplayInfoProviderChromeOS::OverscanCalibrationReset(
    const std::string& id) {
  VLOG(1) << "OverscanCalibrationReset: " << id;
  chromeos::OverscanCalibrator* calibrator = GetOverscanCalibrator(id);
  if (!calibrator)
    return false;
  calibrator->Reset();
  return true;
}

bool DisplayInfoProviderChromeOS::OverscanCalibrationComplete(
    const std::string& id) {
  VLOG(1) << "OverscanCalibrationComplete: " << id;
  chromeos::OverscanCalibrator* calibrator = GetOverscanCalibrator(id);
  if (!calibrator)
    return false;
  calibrator->Commit();
  overscan_calibrators_[id].reset();
  return true;
}

bool DisplayInfoProviderChromeOS::ShowNativeTouchCalibration(
    const std::string& id,
    std::string* error,
    const DisplayInfoProvider::TouchCalibrationCallback& callback) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    return false;
  }
  VLOG(1) << "StartNativeTouchCalibration: " << id;

  // If a custom calibration is already running, then throw an error.
  if (custom_touch_calibration_active_) {
    *error = kCustomTouchCalibrationInProgressError;
    return false;
  }

  const display::Display display = GetDisplay(id);
  if (!ValidateParamsForTouchCalibration(id, display, GetTouchCalibrator(),
                                         error)) {
    return false;
  }

  GetTouchCalibrator()->StartCalibration(display, callback);
  return true;
}

bool DisplayInfoProviderChromeOS::StartCustomTouchCalibration(
    const std::string& id,
    std::string* error) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    return false;
  }
  VLOG(1) << "StartCustomTouchCalibration: " << id;
  const display::Display display = GetDisplay(id);
  if (!ValidateParamsForTouchCalibration(id, display, GetTouchCalibrator(),
                                         error)) {
    return false;
  }

  touch_calibration_target_id_ = id;
  custom_touch_calibration_active_ = true;

  // Enable un-transformed touch input.
  ash::Shell::Get()->touch_transformer_controller()->SetForCalibration(true);
  return true;
}

bool DisplayInfoProviderChromeOS::CompleteCustomTouchCalibration(
    const api::system_display::TouchCalibrationPairQuad& pairs,
    const api::system_display::Bounds& bounds,
    std::string* error) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    return false;
  }
  VLOG(1) << "CompleteCustomTouchCalibration: " << touch_calibration_target_id_;

  ash::Shell::Get()->touch_transformer_controller()->SetForCalibration(false);

  const display::Display display = GetDisplay(touch_calibration_target_id_);
  touch_calibration_target_id_.clear();

  // If Complete() is called before calling Start(), throw an error.
  if (!custom_touch_calibration_active_) {
    *error = kCompleteCalibrationCalledBeforeStartError;
    return false;
  }

  custom_touch_calibration_active_ = false;

  if (!ValidateParamsForTouchCalibration(touch_calibration_target_id_, display,
                                         GetTouchCalibrator(), error)) {
    return false;
  }

  display::TouchCalibrationData::CalibrationPointPairQuad calibration_points;
  calibration_points[0] = GetCalibrationPair(pairs.pair1);
  calibration_points[1] = GetCalibrationPair(pairs.pair2);
  calibration_points[2] = GetCalibrationPair(pairs.pair3);
  calibration_points[3] = GetCalibrationPair(pairs.pair4);

  // The display bounds cannot have negative values.
  if (bounds.width < 0 || bounds.height < 0) {
    *error = kTouchBoundsNegativeError;
    return false;
  }

  for (size_t row = 0; row < calibration_points.size(); row++) {
    // Coordinates for display and touch point cannot be negative.
    if (calibration_points[row].first.x() < 0 ||
        calibration_points[row].first.y() < 0 ||
        calibration_points[row].second.x() < 0 ||
        calibration_points[row].second.y() < 0) {
      *error = kTouchCalibrationPointsNegativeError;
      return false;
    }
    // Coordinates for display points cannot be greater than the screen bounds.
    if (calibration_points[row].first.x() > bounds.width ||
        calibration_points[row].first.y() > bounds.height) {
      *error = kTouchCalibrationPointsTooLargeError;
      return false;
    }
  }

  gfx::Size display_size(bounds.width, bounds.height);
  ash::Shell::Get()->display_manager()->SetTouchCalibrationData(
      display.id(), calibration_points, display_size);
  return true;
}

bool DisplayInfoProviderChromeOS::ClearTouchCalibration(const std::string& id,
                                                        std::string* error) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    *error = "Not implemented for mash.";
    return false;
  }
  const display::Display display = GetDisplay(id);

  if (!ValidateParamsForTouchCalibration(id, display, GetTouchCalibrator(),
                                         error)) {
    return false;
  }

  ash::Shell::Get()->display_manager()->ClearTouchCalibrationData(display.id());
  return true;
}

bool DisplayInfoProviderChromeOS::IsNativeTouchCalibrationActive(
    std::string* error) {
  // If native touch calibration UX is active, set error and return false.
  if (GetTouchCalibrator()->is_calibrating()) {
    *error = kNativeTouchCalibrationActiveError;
    return true;
  }
  return false;
}

chromeos::OverscanCalibrator*
DisplayInfoProviderChromeOS::GetOverscanCalibrator(const std::string& id) {
  auto iter = overscan_calibrators_.find(id);
  if (iter == overscan_calibrators_.end())
    return nullptr;
  return iter->second.get();
}

chromeos::TouchCalibratorController*
DisplayInfoProviderChromeOS::GetTouchCalibrator() {
  if (!touch_calibrator_)
    touch_calibrator_.reset(new chromeos::TouchCalibratorController);
  return touch_calibrator_.get();
}

// static
DisplayInfoProvider* DisplayInfoProvider::Create() {
  return new DisplayInfoProviderChromeOS();
}

}  // namespace extensions
