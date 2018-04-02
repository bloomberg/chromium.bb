// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/display_info_provider_chromeos.h"

#include <stdint.h>
#include <cmath>

#include "ash/display/display_configuration_controller.h"
#include "ash/display/overscan_calibrator.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/touch_calibrator_controller.h"
#include "ash/shell.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/display/display_prefs.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "extensions/common/api/system_display.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/chromeos/display_util.h"
#include "ui/display/manager/chromeos/touch_device_manager.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_utilities.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/unified_desktop_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {

namespace system_display = api::system_display;
using MirrorParamsErrors = display::MixedMirrorModeParamsErrors;

namespace {

// Maximum allowed bounds origin absolute value.
const int kMaxBoundsOrigin = 200 * 1000;

// This is the default range of display width in logical pixels allowed after
// applying display zoom.
constexpr int kDefaultMaxZoomWidth = 4096;
constexpr int kDefaultMinZoomWidth = 640;

// Gets the display with the provided string id.
display::Display GetDisplay(const std::string& display_id_str) {
  int64_t display_id;
  if (!base::StringToInt64(display_id_str, &display_id))
    return display::Display();
  const auto* display_manager = ash::Shell::Get()->display_manager();
  if (display_manager->IsInUnifiedMode() &&
      display_id != display::kUnifiedDisplayId) {
    // In unified desktop mode, the mirroring displays, which constitue the
    // combined unified display, are contained in the software mirroring
    // displays list in the display manager.
    // The active list of displays only contains a single unified display entry
    // with id |kUnifiedDisplayId|.
    return display_manager->GetMirroringDisplayById(display_id);
  }

  return display_manager->GetDisplayForId(display_id);
}

// Returns a |DisplayLayoutList| representation of the unified desktop display
// matrix.
DisplayInfoProvider::DisplayLayoutList GetUnifiedDisplayLayout() {
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  DCHECK(display_manager->IsInUnifiedMode());

  const display::UnifiedDesktopLayoutMatrix& matrix =
      display_manager->current_unified_desktop_matrix();
  DisplayInfoProvider::DisplayLayoutList result;
  for (size_t row_index = 0; row_index < matrix.size(); ++row_index) {
    const auto& row = matrix[row_index];
    for (size_t column_index = 0; column_index < row.size(); ++column_index) {
      if (column_index == 0 && row_index == 0) {
        // No placement for the primary display.
        continue;
      }

      const int64_t display_id = row[column_index];
      system_display::DisplayLayout display_layout;
      display_layout.id = base::Int64ToString(display_id);
      // Parent display is either the one in the above row, or the one on the
      // left in the same row.
      const int64_t parent_id = column_index == 0
                                    ? matrix[row_index - 1][column_index]
                                    : row[column_index - 1];
      display_layout.parent_id = base::Int64ToString(parent_id);
      display_layout.position =
          column_index == 0 ? api::system_display::LAYOUT_POSITION_BOTTOM
                            : api::system_display::LAYOUT_POSITION_RIGHT;
      display_layout.offset = 0;
      result.emplace_back(std::move(display_layout));
    }
  }

  return result;
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

// Updates the display layout for the target display in reference to the primary
// display.
void UpdateDisplayLayout(const gfx::Rect& primary_display_bounds,
                         int64_t primary_display_id,
                         const gfx::Rect& target_display_bounds,
                         int64_t target_display_id) {
  display::DisplayPlacement placement(
      display::DisplayLayout::CreatePlacementForRectangles(
          primary_display_bounds, target_display_bounds));
  placement.display_id = target_display_id;
  placement.parent_display_id = primary_display_id;

  std::unique_ptr<display::DisplayLayout> layout(new display::DisplayLayout);
  layout->placement_list.push_back(placement);
  layout->primary_id = primary_display_id;

  ash::Shell::Get()->display_configuration_controller()->SetDisplayLayout(
      std::move(layout));
}

// Validates that DisplayProperties input is valid. Does not perform any tests
// with DisplayManager dependencies. Returns an error string on failure or an
// empty string on success.
std::string ValidateDisplayPropertiesInput(
    const std::string& display_id_str,
    const system_display::DisplayProperties& info) {
  int64_t id;
  if (!base::StringToInt64(display_id_str, &id))
    return "Invalid display id";

  const display::Display& primary =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  bool is_primary = id == primary.id() || (info.is_primary && *info.is_primary);

  if (info.is_unified) {
    if (!is_primary)
      return "Unified desktop mode can only be set for the primary display.";
    if (info.mirroring_source_id)
      return "Unified desktop mode can not be set with mirroringSourceId.";
  }

  // If mirroring source parameter is specified, no other parameter should be
  // set as when the mirroring is applied the display list could change.
  if (info.mirroring_source_id &&
      (info.is_primary || info.bounds_origin_x || info.bounds_origin_y ||
       info.rotation || info.overscan)) {
    return "No other parameter should be set alongside mirroringSourceId.";
  }

  // The bounds cannot be changed for the primary display and should be inside
  // a reasonable bounds.
  if (info.bounds_origin_x || info.bounds_origin_y) {
    if (is_primary)
      return "Bounds origin not allowed for the primary display.";
    if (info.bounds_origin_x && (*info.bounds_origin_x > kMaxBoundsOrigin ||
                                 *info.bounds_origin_x < -kMaxBoundsOrigin)) {
      return "Bounds origin x out of bounds.";
    }
    if (info.bounds_origin_y && (*info.bounds_origin_y > kMaxBoundsOrigin ||
                                 *info.bounds_origin_y < -kMaxBoundsOrigin)) {
      return "Bounds origin y out of bounds.";
    }
  }

  // Verify the rotation value is valid.
  if (info.rotation && !IsValidRotationValue(*info.rotation))
    return "Invalid rotation.";

  return "";
}

// Validates that DisplayProperties are valid with the current DisplayManager
// configuration. Returns an error string on failure or an empty string on
// success.
std::string ValidateDisplayProperties(
    const system_display::DisplayProperties& info,
    const display::Display& display,
    display::DisplayManager* display_manager) {
  const int64_t id = display.id();
  if (id == display::kInvalidDisplayId)
    return "Display not found.";

  if (info.mirroring_source_id && !info.mirroring_source_id->empty()) {
    // A display with the given id should exist and should not be the same as
    // the target display's id.
    const int64_t mirroring_id = GetDisplay(*info.mirroring_source_id).id();
    if (mirroring_id == display::kInvalidDisplayId)
      return "Invalid mirroring source id";

    if (mirroring_id == id)
      return "Not allowed to mirror self";
  }

  // Overscan cannot be changed for the internal display, and should be at most
  // half of the screen size.
  if (info.overscan) {
    if (display.IsInternal())
      return "Overscan changes not allowed for the internal monitor.";

    if (info.overscan->left < 0 || info.overscan->top < 0 ||
        info.overscan->right < 0 || info.overscan->bottom < 0) {
      return "Negative overscan not allowed.";
    }

    const gfx::Insets overscan = display_manager->GetOverscanInsets(id);
    int screen_width = display.bounds().width() + overscan.width();
    int screen_height = display.bounds().height() + overscan.height();

    if ((info.overscan->left + info.overscan->right) * 2 > screen_width)
      return "Horizontal overscan is more than half of the screen width.";

    if ((info.overscan->top + info.overscan->bottom) * 2 > screen_height)
      return "Vertical overscan is more than half of the screen height.";
  }

  if (info.display_zoom_factor) {
    display::ManagedDisplayMode current_mode;
    if (!display_manager->GetActiveModeForDisplayId(id, &current_mode))
      return "No active mode for display id.";

    // This check is added to limit the range of display zoom that can be
    // applied via the system display API. The said range is such that when a
    // display zoom is applied, the final logical width in pixels should lie
    // within the range of 640 pixels and 4096 pixels.
    const int kMaxAllowedWidth =
        std::max(kDefaultMaxZoomWidth, current_mode.size().width());
    const int kMinAllowedWidth =
        std::min(kDefaultMinZoomWidth, current_mode.size().width());
    int current_width = static_cast<float>(current_mode.size().width()) /
                        current_mode.device_scale_factor();
    if (current_width / (*info.display_zoom_factor) > kMaxAllowedWidth ||
        current_width / (*info.display_zoom_factor) < kMinAllowedWidth) {
      return "Zoom value is out of range for display.";
    }
  }

  return "";
}

// Attempts to set the display mode for display |id|. Returns an error string on
// failure or an empty string on success.
std::string SetDisplayMode(display::DisplayManager* display_manager,
                           int64_t id,
                           const system_display::DisplayProperties& info) {
  if (!info.display_mode)
    return "";  // Nothing to set.

  display::ManagedDisplayMode current_mode;
  if (!display_manager->GetActiveModeForDisplayId(id, &current_mode))
    return "No active mode for display id.";

  // Copy properties not set in the UI from the current mode.
  const gfx::Size size(info.display_mode->width_in_native_pixels,
                       info.display_mode->height_in_native_pixels);

  // NB: info.display_mode is neither a display::ManagedDisplayMode or a
  // display::DisplayMode.
  const display::ManagedDisplayMode new_mode(
      size, current_mode.refresh_rate(), current_mode.is_interlaced(),
      info.display_mode->is_native, info.display_mode->ui_scale,
      info.display_mode->device_scale_factor);

  if (new_mode.IsEquivalent(current_mode))
    return "";  // Do nothing but do not return an error.

  // If it's the internal display, the display mode will be applied directly,
  // otherwise a confirm/revert notification will be prepared first, and the
  // display mode will be applied. If the user accepts the mode change by
  // dismissing the notification, StoreDisplayPrefs() will be called back to
  // persist the new preferences.
  if (!ash::Shell::Get()
           ->resolution_notification_controller()
           ->PrepareNotificationAndSetDisplayMode(
               id, current_mode, new_mode, base::BindRepeating([]() {
                 chromeos::DisplayPrefs::Get()->StoreDisplayPrefs();
               }))) {
    return "Failed to set display mode.";
  }
  return "";
}

system_display::DisplayMode GetDisplayMode(
    display::DisplayManager* display_manager,
    const display::ManagedDisplayInfo& display_info,
    const display::ManagedDisplayMode& display_mode) {
  system_display::DisplayMode result;

  bool is_internal = display::Display::HasInternalDisplay() &&
                     display::Display::InternalDisplayId() == display_info.id();
  gfx::Size size_dip = display_mode.GetSizeInDIP(is_internal);
  result.width = size_dip.width();
  result.height = size_dip.height();
  result.width_in_native_pixels = display_mode.size().width();
  result.height_in_native_pixels = display_mode.size().height();
  result.ui_scale = display_mode.ui_scale();
  result.device_scale_factor = display_mode.device_scale_factor();
  result.is_native = display_mode.native();
  result.refresh_rate = display_mode.refresh_rate();

  display::ManagedDisplayMode mode;
  const bool success =
      display_manager->GetActiveModeForDisplayId(display_info.id(), &mode);
  DCHECK(success);
  result.is_selected = display_mode.IsEquivalent(mode);

  return result;
}

display::TouchCalibrationData::CalibrationPointPair GetCalibrationPair(
    const system_display::TouchCalibrationPair& pair) {
  return std::make_pair(gfx::Point(pair.display_point.x, pair.display_point.y),
                        gfx::Point(pair.touch_point.x, pair.touch_point.y));
}

bool ValidateParamsForTouchCalibration(const std::string& id,
                                       const display::Display& display,
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

  return true;
}

bool IsTabletModeWindowManagerEnabled() {
  return TabletModeClient::Get()->tablet_mode_enabled();
}

void RunResultCallback(DisplayInfoProvider::ErrorCallback callback,
                       base::Optional<std::string> error) {
  if (error)
    LOG(ERROR) << "API call failed: " << *error;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(error)));
}

constexpr char kTouchBoundsNegativeError[] =
    "Bounds cannot have negative values.";

constexpr char kTouchCalibrationPointsNegativeError[] =
    "Display points and touch points cannot have negative coordinates";

constexpr char kTouchCalibrationPointsTooLargeError[] =
    "Display point coordinates cannot be more than size of the display.";

constexpr char kMirrorModeSourceIdNotSpecifiedError[] =
    "Mirroring source id must be specified for mixed mirror mode.";

constexpr char kMirrorModeDestinationIdsNotSpecifiedError[] =
    "Mirroring destination id must be specified for mixed mirror mode.";

constexpr char kMirrorModeSourceIdBadFormatError[] =
    "Mirroring source id is in incorrect format.";

constexpr char kMirrorModeDestinationIdBadFormatError[] =
    "Mirroring destination id is in incorrect format.";

constexpr char kMirrorModeSingleDisplayError[] =
    "Mirror mode cannot be enabled for a single display.";

constexpr char kMirrorModeSourceIdNotFoundError[] =
    "Mirroring source id cannot be found.";

constexpr char kMirrorModeDestinationIdsEmptyError[] =
    "At least one mirroring destination id must be specified.";

constexpr char kMirrorModeDestinationIdNotFoundError[] =
    "Mirroring destination id cannot be found.";

constexpr char kMirrorModeDuplicateIdError[] =
    "Duplicate display id was found.";

}  // namespace

DisplayInfoProviderChromeOS::DisplayInfoProviderChromeOS() {}

DisplayInfoProviderChromeOS::~DisplayInfoProviderChromeOS() {}

void DisplayInfoProviderChromeOS::SetDisplayProperties(
    const std::string& display_id_str,
    const api::system_display::DisplayProperties& properties,
    ErrorCallback callback) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    RunResultCallback(std::move(callback), "Not implemented for mash");
    return;
  }

  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  ash::DisplayConfigurationController* display_configuration_controller =
      ash::Shell::Get()->display_configuration_controller();

  std::string error =
      ValidateDisplayPropertiesInput(display_id_str, properties);
  if (!error.empty()) {
    RunResultCallback(std::move(callback), std::move(error));
    return;
  }

  const display::Display target = GetDisplay(display_id_str);
  error = ValidateDisplayProperties(properties, target, display_manager);
  if (!error.empty()) {
    RunResultCallback(std::move(callback), std::move(error));
    return;
  }

  // Process 'isUnified' parameter if set.
  if (properties.is_unified) {
    display_manager->SetDefaultMultiDisplayModeForCurrentDisplays(
        *properties.is_unified ? display::DisplayManager::UNIFIED
                               : display::DisplayManager::EXTENDED);
  }

  const display::Display& primary =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  // Process 'isPrimary' parameter.
  if (properties.is_primary && *properties.is_primary &&
      target.id() != primary.id()) {
    display_configuration_controller->SetPrimaryDisplayId(target.id());
  }

  // Process 'mirroringSourceId' parameter.
  if (properties.mirroring_source_id) {
    bool mirror = !properties.mirroring_source_id->empty();
    display_configuration_controller->SetMirrorMode(mirror);
  }

  // Process 'overscan' parameter.
  if (properties.overscan) {
    display_manager->SetOverscanInsets(
        target.id(),
        gfx::Insets(properties.overscan->top, properties.overscan->left,
                    properties.overscan->bottom, properties.overscan->right));
  }

  // Process 'rotation' parameter.
  if (properties.rotation) {
    if (IsTabletModeWindowManagerEnabled() &&
        target.id() == display::Display::InternalDisplayId()) {
      ash::Shell::Get()->screen_orientation_controller()->SetLockToRotation(
          DegreesToRotation(*properties.rotation));
    } else {
      display_configuration_controller->SetDisplayRotation(
          target.id(), DegreesToRotation(*properties.rotation),
          display::Display::RotationSource::USER);
    }
  }

  // Process new display origin parameters.
  gfx::Point new_bounds_origin = target.bounds().origin();
  if (properties.bounds_origin_x)
    new_bounds_origin.set_x(*properties.bounds_origin_x);
  if (properties.bounds_origin_y)
    new_bounds_origin.set_y(*properties.bounds_origin_y);

  if (new_bounds_origin != target.bounds().origin()) {
    gfx::Rect target_bounds = target.bounds();
    target_bounds.Offset(new_bounds_origin.x() - target.bounds().x(),
                         new_bounds_origin.y() - target.bounds().y());
    UpdateDisplayLayout(primary.bounds(), primary.id(), target_bounds,
                        target.id());
  }

  // Update the display zoom.
  if (properties.display_zoom_factor) {
    display_manager->UpdateZoomFactor(target.id(),
                                      *properties.display_zoom_factor);
  }

  // Set the display mode. Note: if this returns an error, other properties
  // will have already been applied.
  error = SetDisplayMode(display_manager, target.id(), properties);
  RunResultCallback(std::move(callback),
                    error.empty()
                        ? base::nullopt
                        : base::make_optional<std::string>(std::move(error)));
}

void DisplayInfoProviderChromeOS::SetDisplayLayout(
    const DisplayLayoutList& layouts,
    ErrorCallback callback) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    RunResultCallback(std::move(callback), "Not implemented for mash");
    return;
  }
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  display::DisplayLayoutBuilder builder(
      display_manager->GetCurrentResolvedDisplayLayout());

  bool have_root = false;
  builder.ClearPlacements();
  std::set<int64_t> placement_ids;
  for (const system_display::DisplayLayout& layout : layouts) {
    display::Display display = GetDisplay(layout.id);
    if (display.id() == display::kInvalidDisplayId) {
      RunResultCallback(std::move(callback),
                        "Invalid layout: display id not found: " + layout.id);
      return;
    }
    display::Display parent = GetDisplay(layout.parent_id);
    if (parent.id() == display::kInvalidDisplayId) {
      if (have_root) {
        RunResultCallback(std::move(callback),
                          "Invalid layout: multiple roots.");
        return;
      }
      have_root = true;
      continue;  // No placement for root (primary) display.
    }

    placement_ids.insert(display.id());
    display::DisplayPlacement::Position position =
        GetDisplayPlacementPosition(layout.position);
    builder.AddDisplayPlacement(display.id(), parent.id(), position,
                                layout.offset);
  }
  std::unique_ptr<display::DisplayLayout> layout = builder.Build();

  if (display_manager->IsInUnifiedMode()) {
    const display::DisplayIdList ids_list =
        display_manager->GetCurrentDisplayIdList();
    for (const auto& id : ids_list) {
      // The primary ID is of that display which has no placement.
      if (placement_ids.count(id) == 0) {
        layout->primary_id = id;
        break;
      }
    }

    DCHECK(layout->primary_id != display::kInvalidDisplayId);

    display::UnifiedDesktopLayoutMatrix matrix;
    if (!display::BuildUnifiedDesktopMatrix(
            display_manager->GetCurrentDisplayIdList(), *layout, &matrix)) {
      RunResultCallback(
          std::move(callback),
          "Invalid unified layout: No proper conversion to a matrix");
      return;
    }

    ash::Shell::Get()
        ->display_configuration_controller()
        ->SetUnifiedDesktopLayoutMatrix(matrix);
    RunResultCallback(std::move(callback), base::nullopt);
    return;
  }

  if (!display::DisplayLayout::Validate(
          display_manager->GetCurrentDisplayIdList(), *layout)) {
    RunResultCallback(std::move(callback), "Invalid layout: Validate failed.");
    return;
  }
  ash::Shell::Get()->display_configuration_controller()->SetDisplayLayout(
      std::move(layout));
  RunResultCallback(std::move(callback), base::nullopt);
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
        base::Int64ToString(display_manager->mirroring_source_id());
    unit->mirroring_destination_ids.clear();
    for (int64_t id : display_manager->GetCurrentDisplayIdList()) {
      if (id != display_manager->mirroring_source_id())
        unit->mirroring_destination_ids.emplace_back(base::Int64ToString(id));
    }
  }

  const display::ManagedDisplayInfo& display_info =
      display_manager->GetDisplayInfo(display.id());
  if (!display_info.manufacturer_id().empty() ||
      !display_info.product_id().empty() ||
      (display_info.year_of_manufacture() !=
       display::kInvalidYearOfManufacture)) {
    unit->edid = std::make_unique<system_display::Edid>();
    if (!display_info.manufacturer_id().empty())
      unit->edid->manufacturer_id = display_info.manufacturer_id();
    if (!display_info.product_id().empty())
      unit->edid->product_id = display_info.product_id();
    if (display_info.year_of_manufacture() !=
        display::kInvalidYearOfManufacture) {
      unit->edid->year_of_manufacture = display_info.year_of_manufacture();
    }
  }

  unit->display_zoom_factor = display_info.zoom_factor();

  display::ManagedDisplayMode active_mode;
  if (display_manager->GetActiveModeForDisplayId(display.id(), &active_mode)) {
    unit->available_display_zoom_factors =
        display::GetDisplayZoomFactors(active_mode);
  } else {
    unit->available_display_zoom_factors.push_back(display_info.zoom_factor());
  }

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

  for (const display::ManagedDisplayMode& display_mode :
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

void DisplayInfoProviderChromeOS::GetAllDisplaysInfo(
    bool single_unified,
    base::OnceCallback<void(DisplayUnitInfoList result)> callback) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  DisplayInfoProvider::DisplayUnitInfoList()));
    return;
  }
  DisplayUnitInfoList all_displays;

  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();

  if (!display_manager->IsInUnifiedMode()) {
    DisplayInfoProvider::GetAllDisplaysInfo(
        single_unified,
        base::BindOnce(
            [](base::OnceCallback<void(DisplayUnitInfoList result)> callback,
               DisplayUnitInfoList result) {
              if (IsTabletModeWindowManagerEnabled()) {
                // Set is_tablet_mode for displays with
                // has_accelerometer_support.
                for (auto& display : result) {
                  if (display.has_accelerometer_support) {
                    display.is_tablet_mode = std::make_unique<bool>(true);
                  }
                }
              }
              std::move(callback).Run(std::move(result));
            },
            std::move(callback)));
    return;
  }

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
    primary_id =
        display_manager->GetPrimaryMirroringDisplayForUnifiedDesktop()->id();
  }

  for (const display::Display& display : displays) {
    system_display::DisplayUnitInfo unit_info =
        CreateDisplayUnitInfo(display, primary_id);
    UpdateDisplayUnitInfoForPlatform(display, &unit_info);
    unit_info.is_unified = true;
    all_displays.push_back(std::move(unit_info));
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(all_displays)));
}

void DisplayInfoProviderChromeOS::GetDisplayLayout(
    base::OnceCallback<void(DisplayLayoutList result)> callback) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  DisplayInfoProvider::DisplayLayoutList()));
    return;
  }
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();

  if (display_manager->IsInUnifiedMode()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), GetUnifiedDisplayLayout()));
    return;
  }

  if (display_manager->num_connected_displays() < 2) {
    // Returns an empty result list for a single display.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), DisplayLayoutList()));
    return;
  }

  DisplayLayoutList result;
  display::Screen* screen = display::Screen::GetScreen();
  std::vector<display::Display> displays = screen->GetAllDisplays();

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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
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
  overscan_calibrators_[id].reset(new ash::OverscanCalibrator(display, insets));
  return true;
}

bool DisplayInfoProviderChromeOS::OverscanCalibrationAdjust(
    const std::string& id,
    const system_display::Insets& delta) {
  VLOG(1) << "OverscanCalibrationAdjust: " << id;
  ash::OverscanCalibrator* calibrator = GetOverscanCalibrator(id);
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
  ash::OverscanCalibrator* calibrator = GetOverscanCalibrator(id);
  if (!calibrator)
    return false;
  calibrator->Reset();
  return true;
}

bool DisplayInfoProviderChromeOS::OverscanCalibrationComplete(
    const std::string& id) {
  VLOG(1) << "OverscanCalibrationComplete: " << id;
  ash::OverscanCalibrator* calibrator = GetOverscanCalibrator(id);
  if (!calibrator)
    return false;
  calibrator->Commit();
  overscan_calibrators_[id].reset();
  return true;
}

void DisplayInfoProviderChromeOS::ShowNativeTouchCalibration(
    const std::string& id,
    ErrorCallback callback) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    RunResultCallback(std::move(callback), "Not implemented for mash");
    return;
  }
  VLOG(1) << "StartNativeTouchCalibration: " << id;

  if (!display::HasExternalTouchscreenDevice()) {
    RunResultCallback(
        std::move(callback),
        "NativeTouchCalibration called with no external touch screen device.");
    return;
  }

  if (custom_touch_calibration_active_) {
    RunResultCallback(std::move(callback),
                      "ShowNativeTouchCalibration called while custom touch "
                      "calibration is active.");
    return;
  }

  std::string error;
  const display::Display display = GetDisplay(id);
  if (!ValidateParamsForTouchCalibration(id, display, &error)) {
    RunResultCallback(std::move(callback), std::move(error));
    return;
  }

  GetTouchCalibrator()->StartCalibration(
      display, false /* is_custom_calibration */,
      base::BindOnce(
          [](ErrorCallback callback, bool result) {
            std::move(callback).Run(
                result ? base::nullopt
                       : base::make_optional<std::string>(
                             "Native touch calibration failed"));
          },
          std::move(callback)));
}

bool DisplayInfoProviderChromeOS::IsNativeTouchCalibrationActive() {
  return GetTouchCalibrator()->IsCalibrating();
}

bool DisplayInfoProviderChromeOS::StartCustomTouchCalibration(
    const std::string& id) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    return false;
  }
  VLOG(1) << "StartCustomTouchCalibration: " << id;
  if (!display::HasExternalTouchscreenDevice()) {
    LOG(ERROR) << "StartCustomTouchCalibration called with no extrenal touch "
                  "screen device.";
    return false;
  }

  const display::Display display = GetDisplay(id);
  std::string error;
  if (!ValidateParamsForTouchCalibration(id, display, &error)) {
    LOG(ERROR) << error;
    return false;
  }
  touch_calibration_target_id_ = id;
  custom_touch_calibration_active_ = true;

  GetTouchCalibrator()->StartCalibration(
      display, true /* is_custom_calibration */, base::Callback<void(bool)>());
  return true;
}

bool DisplayInfoProviderChromeOS::CompleteCustomTouchCalibration(
    const api::system_display::TouchCalibrationPairQuad& pairs,
    const api::system_display::Bounds& bounds) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED();
    return false;
  }
  VLOG(1) << "CompleteCustomTouchCalibration: " << touch_calibration_target_id_;

  if (!custom_touch_calibration_active_) {
    LOG(ERROR) << "CompleteCustomTouchCalibration called while not active.";
    return false;
  }

  ash::Shell::Get()->touch_transformer_controller()->SetForCalibration(false);

  const display::Display display = GetDisplay(touch_calibration_target_id_);
  touch_calibration_target_id_.clear();

  custom_touch_calibration_active_ = false;

  std::string error;
  if (!ValidateParamsForTouchCalibration(touch_calibration_target_id_, display,
                                         &error)) {
    LOG(ERROR) << error;
    return false;
  }

  display::TouchCalibrationData::CalibrationPointPairQuad calibration_points;
  calibration_points[0] = GetCalibrationPair(pairs.pair1);
  calibration_points[1] = GetCalibrationPair(pairs.pair2);
  calibration_points[2] = GetCalibrationPair(pairs.pair3);
  calibration_points[3] = GetCalibrationPair(pairs.pair4);

  // The display bounds cannot have negative values.
  if (bounds.width < 0 || bounds.height < 0) {
    LOG(ERROR) << kTouchBoundsNegativeError;
    return false;
  }

  for (size_t row = 0; row < calibration_points.size(); row++) {
    // Coordinates for display and touch point cannot be negative.
    if (calibration_points[row].first.x() < 0 ||
        calibration_points[row].first.y() < 0 ||
        calibration_points[row].second.x() < 0 ||
        calibration_points[row].second.y() < 0) {
      LOG(ERROR) << kTouchCalibrationPointsNegativeError;
      return false;
    }
    // Coordinates for display points cannot be greater than the screen bounds.
    if (calibration_points[row].first.x() > bounds.width ||
        calibration_points[row].first.y() > bounds.height) {
      LOG(ERROR) << kTouchCalibrationPointsTooLargeError;
      return false;
    }
  }

  gfx::Size display_size(bounds.width, bounds.height);
  GetTouchCalibrator()->CompleteCalibration(calibration_points, display_size);
  return true;
}

bool DisplayInfoProviderChromeOS::ClearTouchCalibration(const std::string& id) {
  if (ash_util::IsRunningInMash()) {
    // TODO(crbug.com/682402): Mash support.
    NOTIMPLEMENTED() << "Not implemented for mash.";
    return false;
  }
  const display::Display display = GetDisplay(id);

  std::string error;
  if (!ValidateParamsForTouchCalibration(id, display, &error)) {
    LOG(ERROR) << error;
    return false;
  }
  ash::Shell::Get()->display_manager()->ClearTouchCalibrationData(
      display.id(), base::nullopt);
  return true;
}

bool DisplayInfoProviderChromeOS::IsCustomTouchCalibrationActive() {
  return custom_touch_calibration_active_;
}

void DisplayInfoProviderChromeOS::SetMirrorMode(
    const api::system_display::MirrorModeInfo& info,
    ErrorCallback callback) {
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();

  if (info.mode == api::system_display::MIRROR_MODE_OFF) {
    display_manager->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
    RunResultCallback(std::move(callback), base::nullopt);
    return;
  }

  if (info.mode == api::system_display::MIRROR_MODE_NORMAL) {
    display_manager->SetMirrorMode(display::MirrorMode::kNormal, base::nullopt);
    RunResultCallback(std::move(callback), base::nullopt);
    return;
  }

  DCHECK(info.mode == api::system_display::MIRROR_MODE_MIXED);
  if (!info.mirroring_source_id) {
    RunResultCallback(std::move(callback),
                      kMirrorModeSourceIdNotSpecifiedError);
    return;
  }

  if (!info.mirroring_destination_ids) {
    RunResultCallback(std::move(callback),
                      kMirrorModeDestinationIdsNotSpecifiedError);
    return;
  }

  int64_t source_id;
  if (!base::StringToInt64(*(info.mirroring_source_id), &source_id)) {
    RunResultCallback(std::move(callback), kMirrorModeSourceIdBadFormatError);
    return;
  }

  display::DisplayIdList destination_ids;
  for (auto& id : *(info.mirroring_destination_ids)) {
    int64_t destination_id;
    if (!base::StringToInt64(id, &destination_id)) {
      RunResultCallback(std::move(callback),
                        kMirrorModeDestinationIdBadFormatError);
      return;
    }
    destination_ids.emplace_back(destination_id);
  }

  base::Optional<display::MixedMirrorModeParams> mixed_params(
      base::in_place, source_id, destination_ids);
  const MirrorParamsErrors error_type =
      display::ValidateParamsForMixedMirrorMode(
          display_manager->GetCurrentDisplayIdList(), *mixed_params);
  std::string error;
  switch (error_type) {
    case MirrorParamsErrors::kErrorSingleDisplay:
      error = kMirrorModeSingleDisplayError;
      break;
    case MirrorParamsErrors::kErrorSourceIdNotFound:
      error = kMirrorModeSourceIdNotFoundError;
      break;
    case MirrorParamsErrors::kErrorDestinationIdsEmpty:
      error = kMirrorModeDestinationIdsEmptyError;
      break;
    case MirrorParamsErrors::kErrorDestinationIdNotFound:
      error = kMirrorModeDestinationIdNotFoundError;
      break;
    case MirrorParamsErrors::kErrorDuplicateId:
      error = kMirrorModeDuplicateIdError;
      break;
    case MirrorParamsErrors::kSuccess:
      display_manager->SetMirrorMode(display::MirrorMode::kMixed, mixed_params);
  }
  RunResultCallback(std::move(callback), std::move(error));
}

ash::OverscanCalibrator* DisplayInfoProviderChromeOS::GetOverscanCalibrator(
    const std::string& id) {
  auto iter = overscan_calibrators_.find(id);
  if (iter == overscan_calibrators_.end())
    return nullptr;
  return iter->second.get();
}

ash::TouchCalibratorController*
DisplayInfoProviderChromeOS::GetTouchCalibrator() {
  if (!touch_calibrator_)
    touch_calibrator_.reset(new ash::TouchCalibratorController);
  return touch_calibrator_.get();
}

// static
DisplayInfoProvider* DisplayInfoProvider::Create() {
  return new DisplayInfoProviderChromeOS();
}

}  // namespace extensions
