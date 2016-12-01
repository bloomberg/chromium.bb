// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/display_options_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "ash/common/strings/grit/ash_strings.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace chromeos {
namespace options {
namespace {

display::DisplayManager* GetDisplayManager() {
  return ash::Shell::GetInstance()->display_manager();
}

ash::DisplayConfigurationController* GetDisplayConfigurationController() {
  return ash::Shell::GetInstance()->display_configuration_controller();
}

int64_t GetDisplayIdFromValue(const base::Value* arg) {
  std::string id_value;
  if (!arg->GetAsString(&id_value))
    return display::kInvalidDisplayId;
  int64_t display_id = display::kInvalidDisplayId;
  if (!base::StringToInt64(id_value, &display_id))
    return display::kInvalidDisplayId;
  return display_id;
}

int64_t GetDisplayIdFromArgs(const base::ListValue* args) {
  const base::Value* arg;
  if (!args->Get(0, &arg)) {
    LOG(ERROR) << "No display id arg";
    return display::kInvalidDisplayId;
  }
  int64_t display_id = GetDisplayIdFromValue(arg);
  if (display_id == display::kInvalidDisplayId)
    LOG(ERROR) << "Invalid display id: " << *arg;
  return display_id;
}

int64_t GetDisplayIdFromDictionary(const base::DictionaryValue* dictionary,
                                   const std::string& key) {
  const base::Value* arg;
  if (!dictionary->Get(key, &arg))
    return display::kInvalidDisplayId;
  return GetDisplayIdFromValue(arg);
}

base::string16 GetColorProfileName(ui::ColorCalibrationProfile profile) {
  switch (profile) {
    case ui::COLOR_PROFILE_STANDARD:
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_COLOR_PROFILE_STANDARD);
    case ui::COLOR_PROFILE_DYNAMIC:
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_COLOR_PROFILE_DYNAMIC);
    case ui::COLOR_PROFILE_MOVIE:
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_COLOR_PROFILE_MOVIE);
    case ui::COLOR_PROFILE_READING:
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_COLOR_PROFILE_READING);
    case ui::NUM_COLOR_PROFILES:
      break;
  }

  NOTREACHED();
  return base::string16();
}

int GetIntOrDouble(const base::DictionaryValue* dict,
                   const std::string& field) {
  double double_result = 0;
  if (dict->GetDouble(field, &double_result))
    return static_cast<int>(double_result);

  int result = 0;
  dict->GetInteger(field, &result);
  return result;
}

bool GetFloat(const base::DictionaryValue* dict,
              const std::string& field,
              float* result) {
  double double_result = 0;
  if (dict->GetDouble(field, &double_result)) {
    *result = static_cast<float>(double_result);
    return true;
  }
  return false;
}

scoped_refptr<display::ManagedDisplayMode> ConvertValueToManagedDisplayMode(
    const base::DictionaryValue* dict) {
  scoped_refptr<display::ManagedDisplayMode> mode;

  gfx::Size size;
  size.set_width(GetIntOrDouble(dict, "originalWidth"));
  size.set_height(GetIntOrDouble(dict, "originalHeight"));

  if (size.IsEmpty()) {
    LOG(ERROR) << "missing width or height.";
    return mode;
  }

  float refresh_rate, ui_scale, device_scale_factor;
  if (!GetFloat(dict, "refreshRate", &refresh_rate)) {
    LOG(ERROR) << "missing refreshRate.";
    return mode;
  }
  if (!GetFloat(dict, "scale", &ui_scale)) {
    LOG(ERROR) << "missing ui-scale.";
    return mode;
  }
  if (!GetFloat(dict, "deviceScaleFactor", &device_scale_factor)) {
    LOG(ERROR) << "missing deviceScaleFactor.";
    return mode;
  }

  // Used to select the actual mode.
  mode = new display::ManagedDisplayMode(
      size, refresh_rate, false /* interlaced */, false /* native */, ui_scale,
      device_scale_factor);
  return mode;
}

std::unique_ptr<base::DictionaryValue> ConvertDisplayModeToValue(
    int64_t display_id,
    const scoped_refptr<display::ManagedDisplayMode>& mode) {
  bool is_internal = display::Display::HasInternalDisplay() &&
                     display::Display::InternalDisplayId() == display_id;
  auto result = base::MakeUnique<base::DictionaryValue>();
  gfx::Size size_dip = mode->GetSizeInDIP(is_internal);
  result->SetInteger("width", size_dip.width());
  result->SetInteger("height", size_dip.height());
  result->SetInteger("originalWidth", mode->size().width());
  result->SetInteger("originalHeight", mode->size().height());
  result->SetDouble("deviceScaleFactor", mode->device_scale_factor());
  result->SetDouble("scale", mode->ui_scale());
  result->SetDouble("refreshRate", mode->refresh_rate());
  result->SetBoolean("isBest",
                     is_internal ? (mode->ui_scale() == 1.0f) : mode->native());
  result->SetBoolean("isNative", mode->native());
  result->SetBoolean(
      "selected",
      mode->IsEquivalent(
          GetDisplayManager()->GetActiveModeForDisplayId(display_id)));
  return result;
}

base::DictionaryValue* ConvertBoundsToValue(const gfx::Rect& bounds) {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetInteger("left", bounds.x());
  result->SetInteger("top", bounds.y());
  result->SetInteger("width", bounds.width());
  result->SetInteger("height", bounds.height());
  return result;
}

}  // namespace

DisplayOptionsHandler::DisplayOptionsHandler() {
  // TODO(mash) Support Chrome display settings in Mash. crbug.com/548429
  if (!chrome::IsRunningInMash())
    ash::Shell::GetInstance()->window_tree_host_manager()->AddObserver(this);
}

DisplayOptionsHandler::~DisplayOptionsHandler() {
  // TODO(mash) Support Chrome display settings in Mash. crbug.com/548429
  if (!chrome::IsRunningInMash())
    ash::Shell::GetInstance()->window_tree_host_manager()->RemoveObserver(this);
}

void DisplayOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  RegisterTitle(localized_strings, "displayOptionsPage",
                IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_TAB_TITLE);

  localized_strings->SetString(
      "selectedDisplayTitleOptions", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_OPTIONS));
  localized_strings->SetString(
      "selectedDisplayTitleResolution", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_RESOLUTION));
  localized_strings->SetString(
      "selectedDisplayTitleOrientation", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_ORIENTATION));
  localized_strings->SetString(
      "selectedDisplayTitleOverscan", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_OVERSCAN));

  localized_strings->SetString("extendedMode", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_EXTENDED_MODE_LABEL));
  localized_strings->SetString("mirroringMode", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_MIRRORING_MODE_LABEL));
  localized_strings->SetString("mirroringDisplay", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_MIRRORING_DISPLAY_NAME));
  localized_strings->SetString("setPrimary", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_SET_PRIMARY));
  localized_strings->SetString("annotateBest", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_RESOLUTION_ANNOTATION_BEST));
  localized_strings->SetString("annotateNative", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_RESOLUTION_ANNOTATION_NATIVE));
  localized_strings->SetString("orientation0", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_STANDARD_ORIENTATION));
  localized_strings->SetString("orientation90", l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_ORIENTATION_90));
  localized_strings->SetString("orientation180", l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_ORIENTATION_180));
  localized_strings->SetString("orientation270", l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_ORIENTATION_270));
  localized_strings->SetString(
      "startCalibratingOverscan", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_START_CALIBRATING_OVERSCAN));
  localized_strings->SetString(
      "selectedDisplayColorProfile", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_COLOR_PROFILE));
  localized_strings->SetString(
      "enableUnifiedDesktop",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_ENABLE_UNIFIED_DESKTOP));
}

void DisplayOptionsHandler::InitializePage() {
  DCHECK(web_ui());
  UpdateDisplaySettingsEnabled();
}

void DisplayOptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getDisplayInfo",
      base::Bind(&DisplayOptionsHandler::HandleDisplayInfo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setMirroring",
      base::Bind(&DisplayOptionsHandler::HandleMirroring,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setPrimary",
      base::Bind(&DisplayOptionsHandler::HandleSetPrimary,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setDisplayLayout",
      base::Bind(&DisplayOptionsHandler::HandleSetDisplayLayout,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setDisplayMode",
      base::Bind(&DisplayOptionsHandler::HandleSetDisplayMode,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setRotation",
      base::Bind(&DisplayOptionsHandler::HandleSetRotation,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setColorProfile",
      base::Bind(&DisplayOptionsHandler::HandleSetColorProfile,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setUnifiedDesktopEnabled",
      base::Bind(&DisplayOptionsHandler::HandleSetUnifiedDesktopEnabled,
                 base::Unretained(this)));
}

void DisplayOptionsHandler::OnDisplayConfigurationChanging() {
}

void DisplayOptionsHandler::OnDisplayConfigurationChanged() {
  UpdateDisplaySettingsEnabled();
  SendAllDisplayInfo();
}

void DisplayOptionsHandler::SendAllDisplayInfo() {
  display::DisplayManager* display_manager = GetDisplayManager();

  std::vector<display::Display> displays;
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i)
    displays.push_back(display_manager->GetDisplayAt(i));

  display::DisplayManager::MultiDisplayMode display_mode;
  if (display_manager->IsInMirrorMode())
    display_mode = display::DisplayManager::MIRRORING;
  else if (display_manager->IsInUnifiedMode())
    display_mode = display::DisplayManager::UNIFIED;
  else
    display_mode = display::DisplayManager::EXTENDED;
  base::FundamentalValue mode(static_cast<int>(display_mode));

  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  std::unique_ptr<base::ListValue> js_displays(new base::ListValue);
  for (const display::Display& display : displays) {
    const display::ManagedDisplayInfo& display_info =
        display_manager->GetDisplayInfo(display.id());
    auto js_display = base::MakeUnique<base::DictionaryValue>();
    js_display->SetString("id", base::Int64ToString(display.id()));
    js_display->SetString("name",
                          display_manager->GetDisplayNameForId(display.id()));
    base::DictionaryValue* display_bounds =
        ConvertBoundsToValue(display.bounds());
    js_display->Set("bounds", display_bounds);
    js_display->SetBoolean("isPrimary", display.id() == primary_id);
    js_display->SetBoolean("isInternal", display.IsInternal());
    js_display->SetInteger("rotation", display.RotationAsDegree());

    base::ListValue* js_resolutions = new base::ListValue();
    for (const scoped_refptr<display::ManagedDisplayMode>& display_mode :
         display_info.display_modes()) {
      js_resolutions->Append(
          ConvertDisplayModeToValue(display.id(), display_mode));
    }
    js_display->Set("resolutions", js_resolutions);

    js_display->SetInteger("colorProfileId", display_info.color_profile());
    base::ListValue* available_color_profiles = new base::ListValue();
    for (const auto& color_profile : display_info.available_color_profiles()) {
      const base::string16 profile_name = GetColorProfileName(color_profile);
      if (profile_name.empty())
        continue;
      auto color_profile_dict = base::MakeUnique<base::DictionaryValue>();
      color_profile_dict->SetInteger("profileId", color_profile);
      color_profile_dict->SetString("name", profile_name);
      available_color_profiles->Append(std::move(color_profile_dict));
    }
    js_display->Set("availableColorProfiles", available_color_profiles);

    if (display_manager->GetNumDisplays() > 1) {
      const display::DisplayPlacement placement =
          display_manager->GetCurrentDisplayLayout().FindPlacementById(
              display.id());
      if (placement.display_id != display::kInvalidDisplayId) {
        js_display->SetString(
            "parentId", base::Int64ToString(placement.parent_display_id));
        js_display->SetInteger("layoutType", placement.position);
        js_display->SetInteger("offset", placement.offset);
      }
    }

    js_displays->Append(std::move(js_display));
  }

  web_ui()->CallJavascriptFunctionUnsafe(
      "options.DisplayOptions.setDisplayInfo", mode, *js_displays);
}

void DisplayOptionsHandler::UpdateDisplaySettingsEnabled() {
  // TODO(mash) Support Chrome display settings in Mash. crbug.com/548429
  if (chrome::IsRunningInMash())
    return;

  display::DisplayManager* display_manager = GetDisplayManager();
  bool disable_multi_display_layout =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableMultiDisplayLayout);
  bool ui_enabled = display_manager->num_connected_displays() <= 2 ||
                    !disable_multi_display_layout;
  bool unified_enabled = display_manager->unified_desktop_enabled();
  bool mirrored_enabled = display_manager->num_connected_displays() == 2;

  web_ui()->CallJavascriptFunctionUnsafe(
      "options.BrowserOptions.enableDisplaySettings",
      base::FundamentalValue(ui_enabled),
      base::FundamentalValue(unified_enabled),
      base::FundamentalValue(mirrored_enabled));
}

void DisplayOptionsHandler::HandleDisplayInfo(
    const base::ListValue* unused_args) {
  SendAllDisplayInfo();
}

void DisplayOptionsHandler::HandleMirroring(const base::ListValue* args) {
  DCHECK(!args->empty());
  bool is_mirroring = false;
  if (!args->GetBoolean(0, &is_mirroring))
    NOTREACHED();
  content::RecordAction(
      base::UserMetricsAction("Options_DisplayToggleMirroring"));
  GetDisplayConfigurationController()->SetMirrorMode(is_mirroring,
                                                     true /* user_action */);
}

void DisplayOptionsHandler::HandleSetPrimary(const base::ListValue* args) {
  DCHECK(!args->empty());
  int64_t display_id = GetDisplayIdFromArgs(args);
  if (display_id == display::kInvalidDisplayId)
    return;

  content::RecordAction(base::UserMetricsAction("Options_DisplaySetPrimary"));
  GetDisplayConfigurationController()->SetPrimaryDisplayId(
      display_id, true /* user_action */);
}

void DisplayOptionsHandler::HandleSetDisplayLayout(
    const base::ListValue* args) {
  const base::ListValue* layouts = nullptr;
  if (!args->GetList(0, &layouts))
    NOTREACHED();
  content::RecordAction(base::UserMetricsAction("Options_DisplayRearrange"));

  display::DisplayManager* display_manager = GetDisplayManager();
  display::DisplayLayoutBuilder builder(
      display_manager->GetCurrentDisplayLayout());
  builder.ClearPlacements();
  for (const auto& layout : *layouts) {
    const base::DictionaryValue* dictionary;
    if (!layout->GetAsDictionary(&dictionary)) {
      LOG(ERROR) << "Invalid layout dictionary: " << *dictionary;
      continue;
    }

    int64_t parent_id = GetDisplayIdFromDictionary(dictionary, "parentId");
    if (parent_id == display::kInvalidDisplayId)
      continue;  // No placement for root (primary) display.

    int64_t display_id = GetDisplayIdFromDictionary(dictionary, "id");
    if (display_id == display::kInvalidDisplayId) {
      LOG(ERROR) << "Invalud display id in layout dictionary: " << *dictionary;
      continue;
    }

    int position = 0;
    dictionary->GetInteger("layoutType", &position);
    int offset = 0;
    dictionary->GetInteger("offset", &offset);

    builder.AddDisplayPlacement(
        display_id, parent_id,
        static_cast<display::DisplayPlacement::Position>(position), offset);
  }
  std::unique_ptr<display::DisplayLayout> layout = builder.Build();
  if (!display::DisplayLayout::Validate(
          display_manager->GetCurrentDisplayIdList(), *layout)) {
    LOG(ERROR) << "Invalid layout: " << layout->ToString();
    return;
  }

  VLOG(1) << "Updating display layout: " << layout->ToString();
  GetDisplayConfigurationController()->SetDisplayLayout(std::move(layout),
                                                        true /* user_action */);
}

void DisplayOptionsHandler::HandleSetDisplayMode(const base::ListValue* args) {
  DCHECK(!args->empty());

  int64_t display_id = GetDisplayIdFromArgs(args);
  if (display_id == display::kInvalidDisplayId)
    return;

  const base::DictionaryValue* mode_data = nullptr;
  if (!args->GetDictionary(1, &mode_data)) {
    LOG(ERROR) << "Failed to get mode data";
    return;
  }

  scoped_refptr<display::ManagedDisplayMode> mode =
      ConvertValueToManagedDisplayMode(mode_data);
  if (!mode)
    return;

  content::RecordAction(
      base::UserMetricsAction("Options_DisplaySetResolution"));
  display::DisplayManager* display_manager = GetDisplayManager();
  scoped_refptr<display::ManagedDisplayMode> current_mode =
      display_manager->GetActiveModeForDisplayId(display_id);
  if (!display_manager->SetDisplayMode(display_id, mode)) {
    LOG(ERROR) << "Unable to set display mode for: " << display_id
               << " Mode: " << *mode_data;
    return;
  }
  if (display::Display::IsInternalDisplayId(display_id))
    return;
  // For external displays, show a notification confirming the resolution
  // change.
  ash::Shell::GetInstance()
      ->resolution_notification_controller()
      ->PrepareNotification(display_id, current_mode, mode,
                            base::Bind(&chromeos::StoreDisplayPrefs));
}

void DisplayOptionsHandler::HandleSetRotation(const base::ListValue* args) {
  DCHECK(!args->empty());

  int64_t display_id = GetDisplayIdFromArgs(args);
  if (display_id == display::kInvalidDisplayId)
    return;

  int rotation_value = 0;
  if (!args->GetInteger(1, &rotation_value)) {
    LOG(ERROR) << "Can't parse rotation: " << args;
    return;
  }
  display::Display::Rotation new_rotation = display::Display::ROTATE_0;
  if (rotation_value == 90)
    new_rotation = display::Display::ROTATE_90;
  else if (rotation_value == 180)
    new_rotation = display::Display::ROTATE_180;
  else if (rotation_value == 270)
    new_rotation = display::Display::ROTATE_270;
  else if (rotation_value != 0)
    LOG(ERROR) << "Invalid rotation: " << rotation_value << " Falls back to 0";

  content::RecordAction(
      base::UserMetricsAction("Options_DisplaySetOrientation"));
  GetDisplayConfigurationController()->SetDisplayRotation(
      display_id, new_rotation, display::Display::ROTATION_SOURCE_USER,
      true /* user_action */);
}

void DisplayOptionsHandler::HandleSetColorProfile(const base::ListValue* args) {
  DCHECK(!args->empty());
  int64_t display_id = GetDisplayIdFromArgs(args);
  if (display_id == display::kInvalidDisplayId)
    return;

  std::string profile_value;
  if (!args->GetString(1, &profile_value)) {
    LOG(ERROR) << "Invalid profile_value";
    return;
  }

  int profile_id;
  if (!base::StringToInt(profile_value, &profile_id)) {
    LOG(ERROR) << "Invalid profile: " << profile_value;
    return;
  }

  if (profile_id < ui::COLOR_PROFILE_STANDARD ||
      profile_id > ui::COLOR_PROFILE_READING) {
    LOG(ERROR) << "Invalid profile_id: " << profile_id;
    return;
  }

  content::RecordAction(
      base::UserMetricsAction("Options_DisplaySetColorProfile"));
  GetDisplayManager()->SetColorCalibrationProfile(
      display_id, static_cast<ui::ColorCalibrationProfile>(profile_id));

  SendAllDisplayInfo();
}

void DisplayOptionsHandler::HandleSetUnifiedDesktopEnabled(
    const base::ListValue* args) {
  DCHECK(GetDisplayManager()->unified_desktop_enabled());
  bool enable = false;
  if (!args->GetBoolean(0, &enable))
    NOTREACHED();

  GetDisplayManager()->SetDefaultMultiDisplayModeForCurrentDisplays(
      enable ? display::DisplayManager::UNIFIED
             : display::DisplayManager::EXTENDED);
}

}  // namespace options
}  // namespace chromeos
