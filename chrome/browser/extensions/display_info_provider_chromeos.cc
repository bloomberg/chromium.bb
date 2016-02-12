// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/display_info_provider_chromeos.h"

#include <stdint.h>

#include "ash/display/display_configuration_controller.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "base/strings/string_number_conversions.h"
#include "extensions/common/api/system_display.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {

using api::system_display::Bounds;
using api::system_display::DisplayUnitInfo;
using api::system_display::DisplayProperties;
using api::system_display::Insets;

namespace {

// Maximum allowed bounds origin absolute value.
const int kMaxBoundsOrigin = 200 * 1000;

// Checks if the given integer value is valid display rotation in degrees.
bool IsValidRotationValue(int rotation) {
  return rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270;
}

// Converts integer integer value in degrees to Rotation enum value.
gfx::Display::Rotation DegreesToRotation(int degrees) {
  DCHECK(IsValidRotationValue(degrees));
  switch (degrees) {
    case 0:
      return gfx::Display::ROTATE_0;
    case 90:
      return gfx::Display::ROTATE_90;
    case 180:
      return gfx::Display::ROTATE_180;
    case 270:
      return gfx::Display::ROTATE_270;
    default:
      return gfx::Display::ROTATE_0;
  }
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

// Created ash::DisplayLayout value for |rectangle| compared to the |reference|
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
ash::DisplayPlacement GetPlacementForRectangles(const gfx::Rect& reference,
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

  ash::DisplayPlacement::Position position;
  if (is_top_right) {
    position = is_bottom_right ? ash::DisplayPlacement::RIGHT
                               : ash::DisplayPlacement::TOP;
  } else {
    position = is_bottom_right ? ash::DisplayPlacement::BOTTOM
                               : ash::DisplayPlacement::LEFT;
  }

  // If the rectangle with the calculated position would not have common side
  // with the reference, try to position it so it shares another edge with the
  // reference.
  if (is_top_right == is_bottom_right) {
    if (rectangle.y() > reference.y() + reference.height()) {
      // The rectangle is left or right, but completely under the reference.
      position = ash::DisplayPlacement::BOTTOM;
    } else if (rectangle.y() + rectangle.height() < reference.y()) {
      // The rectangle is left or right, but completely over the reference.
      position = ash::DisplayPlacement::TOP;
    }
  } else {
    if (rectangle.x() > reference.x() + reference.width()) {
      // The rectangle is over or under, but completely right of the reference.
      position = ash::DisplayPlacement::RIGHT;
    } else if (rectangle.x() + rectangle.width() < reference.x()) {
      // The rectangle is over or under, but completely left of the reference.
      position = ash::DisplayPlacement::LEFT;
    }
  }
  int offset = (position == ash::DisplayPlacement::LEFT ||
                position == ash::DisplayPlacement::RIGHT)
                   ? rectangle.y()
                   : rectangle.x();
  return ash::DisplayPlacement(position, offset);
}

// Updates the display layout for the target display in reference to the primary
// display.
void UpdateDisplayLayout(const gfx::Rect& primary_display_bounds,
                         int64_t primary_display_id,
                         const gfx::Rect& target_display_bounds,
                         int64_t target_display_id) {
  ash::DisplayLayout layout;
  layout.placement =
      GetPlacementForRectangles(primary_display_bounds, target_display_bounds);
  layout.primary_id = primary_display_id;
  layout.placement.display_id = target_display_id;
  layout.placement.parent_display_id = primary_display_id;

  ash::Shell::GetInstance()
      ->display_configuration_controller()
      ->SetDisplayLayout(layout, false /* user_action */);
}

// Validates that parameters passed to the SetInfo function are valid for the
// desired display and the current display manager state.
// Returns whether the parameters are valid. On failure |error| is set to the
// error message.
bool ValidateParamsForDisplay(const DisplayProperties& info,
                              const gfx::Display& display,
                              ash::DisplayManager* display_manager,
                              int64_t primary_display_id,
                              std::string* error) {
  bool is_primary = display.id() == primary_display_id ||
                    (info.is_primary && *info.is_primary);

  // If mirroring source id is set, a display with the given id should exist,
  // and if should not be the same as the target display's id.
  if (info.mirroring_source_id && !info.mirroring_source_id->empty()) {
    int64_t mirroring_id;
    if (!base::StringToInt64(*info.mirroring_source_id, &mirroring_id) ||
        display_manager->GetDisplayForId(mirroring_id).id() ==
            gfx::Display::kInvalidDisplayID) {
      *error = "Display " + *info.mirroring_source_id + " not found.";
      return false;
    }

    if (*info.mirroring_source_id == base::Int64ToString(display.id())) {
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

    const gfx::Insets overscan =
        display_manager->GetOverscanInsets(display.id());
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
  return true;
}

// Gets the display with the provided string id.
gfx::Display GetTargetDisplay(const std::string& display_id_str,
                              ash::DisplayManager* manager) {
  int64_t display_id;
  if (!base::StringToInt64(display_id_str, &display_id)) {
    // This should return invalid display.
    return gfx::Display();
  }
  return manager->GetDisplayForId(display_id);
}

}  // namespace

DisplayInfoProviderChromeOS::DisplayInfoProviderChromeOS() {
}

DisplayInfoProviderChromeOS::~DisplayInfoProviderChromeOS() {
}

bool DisplayInfoProviderChromeOS::SetInfo(const std::string& display_id_str,
                                          const DisplayProperties& info,
                                          std::string* error) {
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  ash::DisplayConfigurationController* display_configuration_controller =
      ash::Shell::GetInstance()->display_configuration_controller();

  const gfx::Display target = GetTargetDisplay(display_id_str, display_manager);

  if (target.id() == gfx::Display::kInvalidDisplayID) {
    *error = "Display not found.";
    return false;
  }

  int64_t display_id = target.id();
  const gfx::Display& primary = gfx::Screen::GetScreen()->GetPrimaryDisplay();

  if (!ValidateParamsForDisplay(
          info, target, display_manager, primary.id(), error)) {
    return false;
  }

  // Process 'isPrimary' parameter.
  if (info.is_primary && *info.is_primary && target.id() != primary.id()) {
    display_configuration_controller->SetPrimaryDisplayId(
        display_id, false /* user_action */);
  }

  // Process 'mirroringSourceId' parameter.
  if (info.mirroring_source_id) {
    bool mirror = !info.mirroring_source_id->empty();
    display_configuration_controller->SetMirrorMode(mirror,
                                                    false /* user_action */);
  }

  // Process 'overscan' parameter.
  if (info.overscan) {
    display_manager->SetOverscanInsets(display_id,
                                       gfx::Insets(info.overscan->top,
                                                   info.overscan->left,
                                                   info.overscan->bottom,
                                                   info.overscan->right));
  }

  // Process 'rotation' parameter.
  if (info.rotation) {
    display_configuration_controller->SetDisplayRotation(
        display_id, DegreesToRotation(*info.rotation),
        gfx::Display::ROTATION_SOURCE_ACTIVE, false /* user_action */);
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
    UpdateDisplayLayout(
        primary.bounds(), primary.id(), target_bounds, target.id());
  }

  return true;
}

void DisplayInfoProviderChromeOS::UpdateDisplayUnitInfoForPlatform(
    const gfx::Display& display,
    extensions::api::system_display::DisplayUnitInfo* unit) {
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  unit->name = display_manager->GetDisplayNameForId(display.id());
  if (display_manager->IsInMirrorMode()) {
    unit->mirroring_source_id =
        base::Int64ToString(display_manager->mirroring_display_id());
  }

  const ash::DisplayInfo& display_info =
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
}

void DisplayInfoProviderChromeOS::EnableUnifiedDesktop(bool enable) {
  ash::Shell::GetInstance()->display_manager()->SetUnifiedDesktopEnabled(
      enable);
}

DisplayInfo DisplayInfoProviderChromeOS::GetAllDisplaysInfo() {
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  if (!display_manager->IsInUnifiedMode())
    return DisplayInfoProvider::GetAllDisplaysInfo();

  std::vector<gfx::Display> displays =
      display_manager->software_mirroring_display_list();
  CHECK_GT(displays.size(), 0u);

  // Use first display as primary.
  int64_t primary_id = displays[0].id();
  DisplayInfo all_displays;
  for (const gfx::Display& display : displays) {
    linked_ptr<api::system_display::DisplayUnitInfo> unit(
        CreateDisplayUnitInfo(display, primary_id));
    UpdateDisplayUnitInfoForPlatform(display, unit.get());
    all_displays.push_back(unit);
  }
  return all_displays;
}

// static
DisplayInfoProvider* DisplayInfoProvider::Create() {
  return new DisplayInfoProviderChromeOS();
}

}  // namespace extensions
