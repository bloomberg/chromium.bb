// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/display_options_handler.h"

#include <string>

#include "ash/display/display_configurator_animation.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "grit/ash_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"

using ash::DisplayManager;

namespace chromeos {
namespace options {
namespace {

DisplayManager* GetDisplayManager() {
  return ash::Shell::GetInstance()->display_manager();
}

int64 GetDisplayId(const base::ListValue* args) {
  // Assumes the display ID is specified as the first argument.
  std::string id_value;
  if (!args->GetString(0, &id_value)) {
    LOG(ERROR) << "Can't find ID";
    return gfx::Display::kInvalidDisplayID;
  }

  int64 display_id = gfx::Display::kInvalidDisplayID;
  if (!base::StringToInt64(id_value, &display_id)) {
    LOG(ERROR) << "Invalid display id: " << id_value;
    return gfx::Display::kInvalidDisplayID;
  }

  return display_id;
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

bool ConvertValueToDisplayMode(const base::DictionaryValue* dict,
                               ash::DisplayMode* mode) {
  mode->size.set_width(GetIntOrDouble(dict, "originalWidth"));
  mode->size.set_height(GetIntOrDouble(dict, "originalHeight"));
  if (mode->size.IsEmpty()) {
    LOG(ERROR) << "missing width or height.";
    return false;
  }
  if (!GetFloat(dict, "refreshRate", &mode->refresh_rate)) {
    LOG(ERROR) << "missing refreshRate.";
    return false;
  }
  if (!GetFloat(dict, "scale", &mode->ui_scale)) {
    LOG(ERROR) << "missing ui-scale.";
    return false;
  }
  if (!GetFloat(dict, "deviceScaleFactor", &mode->device_scale_factor)) {
    LOG(ERROR) << "missing deviceScaleFactor.";
    return false;
  }
  return true;
}

base::DictionaryValue* ConvertDisplayModeToValue(int64 display_id,
                                                 const ash::DisplayMode& mode) {
  base::DictionaryValue* result = new base::DictionaryValue();
  gfx::Size size_dip = mode.GetSizeInDIP();
  result->SetInteger("width", size_dip.width());
  result->SetInteger("height", size_dip.height());
  result->SetInteger("originalWidth", mode.size.width());
  result->SetInteger("originalHeight", mode.size.height());
  result->SetDouble("deviceScaleFactor", mode.device_scale_factor);
  result->SetDouble("scale", mode.ui_scale);
  result->SetDouble("refreshRate", mode.refresh_rate);
  result->SetBoolean("isBest", mode.native);
  result->SetBoolean(
      "selected", mode.IsEquivalent(
          GetDisplayManager()->GetActiveModeForDisplayId(display_id)));
  return result;
}

}  // namespace

DisplayOptionsHandler::DisplayOptionsHandler() {
  ash::Shell::GetInstance()->display_controller()->AddObserver(this);
}

DisplayOptionsHandler::~DisplayOptionsHandler() {
  ash::Shell::GetInstance()->display_controller()->RemoveObserver(this);
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

  localized_strings->SetString("startMirroring", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_START_MIRRORING));
  localized_strings->SetString("stopMirroring", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_STOP_MIRRORING));
  localized_strings->SetString("mirroringDisplay", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_MIRRORING_DISPLAY_NAME));
  localized_strings->SetString("setPrimary", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_SET_PRIMARY));
  localized_strings->SetString("annotateBest", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_RESOLUTION_ANNOTATION_BEST));
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
}

void DisplayOptionsHandler::InitializePage() {
  DCHECK(web_ui());
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
      base::Bind(&DisplayOptionsHandler::HandleDisplayLayout,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setDisplayMode",
      base::Bind(&DisplayOptionsHandler::HandleSetDisplayMode,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setOrientation",
      base::Bind(&DisplayOptionsHandler::HandleSetOrientation,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setColorProfile",
      base::Bind(&DisplayOptionsHandler::HandleSetColorProfile,
                 base::Unretained(this)));
}

void DisplayOptionsHandler::OnDisplayConfigurationChanging() {
}

void DisplayOptionsHandler::OnDisplayConfigurationChanged() {
  SendAllDisplayInfo();
}

void DisplayOptionsHandler::SendAllDisplayInfo() {
  DisplayManager* display_manager = GetDisplayManager();

  std::vector<gfx::Display> displays;
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    displays.push_back(display_manager->GetDisplayAt(i));
  }
  SendDisplayInfo(displays);
}

void DisplayOptionsHandler::SendDisplayInfo(
    const std::vector<gfx::Display>& displays) {
  DisplayManager* display_manager = GetDisplayManager();
  base::FundamentalValue mirroring(display_manager->IsMirrored());

  int64 primary_id = ash::Shell::GetScreen()->GetPrimaryDisplay().id();
  base::ListValue js_displays;
  for (size_t i = 0; i < displays.size(); ++i) {
    const gfx::Display& display = displays[i];
    const ash::DisplayInfo& display_info =
        display_manager->GetDisplayInfo(display.id());
    const gfx::Rect& bounds = display.bounds();
    base::DictionaryValue* js_display = new base::DictionaryValue();
    js_display->SetString("id", base::Int64ToString(display.id()));
    js_display->SetInteger("x", bounds.x());
    js_display->SetInteger("y", bounds.y());
    js_display->SetInteger("width", bounds.width());
    js_display->SetInteger("height", bounds.height());
    js_display->SetString("name",
                          display_manager->GetDisplayNameForId(display.id()));
    js_display->SetBoolean("isPrimary", display.id() == primary_id);
    js_display->SetBoolean("isInternal", display.IsInternal());
    js_display->SetInteger("orientation",
                           static_cast<int>(display_info.rotation()));

    base::ListValue* js_resolutions = new base::ListValue();
    const std::vector<ash::DisplayMode>& display_modes =
        display_info.display_modes();
    for (size_t i = 0; i < display_modes.size(); ++i) {
      js_resolutions->Append(
          ConvertDisplayModeToValue(display.id(), display_modes[i]));
    }
    js_display->Set("resolutions", js_resolutions);

    js_display->SetInteger("colorProfile", display_info.color_profile());
    base::ListValue* available_color_profiles = new base::ListValue();
    for (size_t i = 0;
         i < display_info.available_color_profiles().size(); ++i) {
      base::DictionaryValue* color_profile_dict = new base::DictionaryValue();
      ui::ColorCalibrationProfile color_profile =
          display_info.available_color_profiles()[i];
      color_profile_dict->SetInteger("profileId", color_profile);
      color_profile_dict->SetString("name", GetColorProfileName(color_profile));
      available_color_profiles->Append(color_profile_dict);
    }
    js_display->Set("availableColorProfiles", available_color_profiles);
    js_displays.Append(js_display);
  }

  scoped_ptr<base::Value> layout_value(base::Value::CreateNullValue());
  scoped_ptr<base::Value> offset_value(base::Value::CreateNullValue());
  if (display_manager->GetNumDisplays() > 1) {
    const ash::DisplayLayout layout =
        display_manager->GetCurrentDisplayLayout();
    layout_value.reset(new base::FundamentalValue(layout.position));
    offset_value.reset(new base::FundamentalValue(layout.offset));
  }

  web_ui()->CallJavascriptFunction(
      "options.DisplayOptions.setDisplayInfo",
      mirroring, js_displays, *layout_value.get(), *offset_value.get());
}

void DisplayOptionsHandler::OnFadeOutForMirroringFinished(bool is_mirroring) {
  ash::Shell::GetInstance()->display_manager()->SetMirrorMode(is_mirroring);
  // Not necessary to start fade-in animation. DisplayConfigurator will do that.
}

void DisplayOptionsHandler::OnFadeOutForDisplayLayoutFinished(
    int position, int offset) {
  SetCurrentDisplayLayout(
      ash::DisplayLayout::FromInts(position, offset));
  ash::Shell::GetInstance()->display_configurator_animation()->
      StartFadeInAnimation();
}

void DisplayOptionsHandler::HandleDisplayInfo(
    const base::ListValue* unused_args) {
  SendAllDisplayInfo();
}

void DisplayOptionsHandler::HandleMirroring(const base::ListValue* args) {
  DCHECK(!args->empty());
  content::RecordAction(
      base::UserMetricsAction("Options_DisplayToggleMirroring"));
  bool is_mirroring = false;
  args->GetBoolean(0, &is_mirroring);
  ash::Shell::GetInstance()->display_configurator_animation()->
      StartFadeOutAnimation(base::Bind(
          &DisplayOptionsHandler::OnFadeOutForMirroringFinished,
          base::Unretained(this),
          is_mirroring));
}

void DisplayOptionsHandler::HandleSetPrimary(const base::ListValue* args) {
  DCHECK(!args->empty());
  int64 display_id = GetDisplayId(args);
  if (display_id == gfx::Display::kInvalidDisplayID)
    return;

  content::RecordAction(base::UserMetricsAction("Options_DisplaySetPrimary"));
  ash::Shell::GetInstance()->display_controller()->
      SetPrimaryDisplayId(display_id);
}

void DisplayOptionsHandler::HandleDisplayLayout(const base::ListValue* args) {
  double layout = -1;
  double offset = -1;
  if (!args->GetDouble(0, &layout) || !args->GetDouble(1, &offset)) {
    LOG(ERROR) << "Invalid parameter";
    SendAllDisplayInfo();
    return;
  }
  DCHECK_LE(ash::DisplayLayout::TOP, layout);
  DCHECK_GE(ash::DisplayLayout::LEFT, layout);
  content::RecordAction(base::UserMetricsAction("Options_DisplayRearrange"));
  ash::Shell::GetInstance()->display_configurator_animation()->
      StartFadeOutAnimation(base::Bind(
          &DisplayOptionsHandler::OnFadeOutForDisplayLayoutFinished,
          base::Unretained(this),
          static_cast<int>(layout),
          static_cast<int>(offset)));
}

void DisplayOptionsHandler::HandleSetDisplayMode(const base::ListValue* args) {
  DCHECK(!args->empty());

  int64 display_id = GetDisplayId(args);
  if (display_id == gfx::Display::kInvalidDisplayID)
    return;

  const base::DictionaryValue* mode_data = NULL;
  if (!args->GetDictionary(1, &mode_data)) {
    LOG(ERROR) << "Failed to get mode data";
    return;
  }

  ash::DisplayMode mode;
  if (!ConvertValueToDisplayMode(mode_data, &mode))
    return;

  content::RecordAction(
      base::UserMetricsAction("Options_DisplaySetResolution"));
  ash::DisplayManager* display_manager = GetDisplayManager();
  ash::DisplayMode current_mode =
      display_manager->GetActiveModeForDisplayId(display_id);
  if (display_manager->SetDisplayMode(display_id, mode)) {
    ash::Shell::GetInstance()->resolution_notification_controller()->
        PrepareNotification(
            display_id, current_mode, mode, base::Bind(&StoreDisplayPrefs));
  }
}

void DisplayOptionsHandler::HandleSetOrientation(const base::ListValue* args) {
  DCHECK(!args->empty());

  int64 display_id = GetDisplayId(args);
  if (display_id == gfx::Display::kInvalidDisplayID)
    return;

  std::string rotation_value;
  gfx::Display::Rotation new_rotation = gfx::Display::ROTATE_0;
  if (!args->GetString(1, &rotation_value)) {
    LOG(ERROR) << "Can't find new orientation";
    return;
  }
  if (rotation_value == "90")
    new_rotation = gfx::Display::ROTATE_90;
  else if (rotation_value == "180")
    new_rotation = gfx::Display::ROTATE_180;
  else if (rotation_value == "270")
    new_rotation = gfx::Display::ROTATE_270;
  else if (rotation_value != "0")
    LOG(ERROR) << "Invalid rotation: " << rotation_value << " Falls back to 0";

  content::RecordAction(
      base::UserMetricsAction("Options_DisplaySetOrientation"));
  GetDisplayManager()->SetDisplayRotation(display_id, new_rotation);
}

void DisplayOptionsHandler::HandleSetColorProfile(const base::ListValue* args) {
  DCHECK(!args->empty());
  int64 display_id = GetDisplayId(args);
  if (display_id == gfx::Display::kInvalidDisplayID)
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

  GetDisplayManager()->SetColorCalibrationProfile(
      display_id, static_cast<ui::ColorCalibrationProfile>(profile_id));
  SendAllDisplayInfo();
}

}  // namespace options
}  // namespace chromeos
