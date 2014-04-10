// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/display_options_handler.h"

#include <string>

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/display/output_configurator_animation.h"
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

bool CompareResolution(base::Value* display1, base::Value* display2) {
  base::DictionaryValue* d1 = NULL;
  base::DictionaryValue* d2 = NULL;
  CHECK(display1->GetAsDictionary(&d1) && display2->GetAsDictionary(&d2));
  int width1 = 0, height1 = 0, width2 = 0, height2 = 0;
  CHECK(d1->GetInteger("width", &width1) && d1->GetInteger("height", &height1));
  CHECK(d2->GetInteger("width", &width2) && d2->GetInteger("height", &height2));
  double scale_factor1 = 0, scale_factor2 = 0;
  if (d1->GetDouble("scaleFactor", &scale_factor1)) {
    width1 /= scale_factor1;
    height1 /= scale_factor1;
  }
  if (d2->GetDouble("scaleFactor", &scale_factor2)) {
    width2 /= scale_factor2;
    height2 /= scale_factor2;
  }

  if (width1 * height1 == width2 * height2) {
    if (scale_factor1 != scale_factor2)
      return scale_factor1 < scale_factor2;

    int refresh_rate1 = 0, refresh_rate2 = 0;
    CHECK(d1->GetInteger("refreshRate", &refresh_rate1) ==
          d2->GetInteger("refreshRate", &refresh_rate2));
    return refresh_rate1 < refresh_rate2;
  }
  return width1 * height1 < width2 * height2;
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

scoped_ptr<base::ListValue> GetResolutionsForInternalDisplay(
    const ash::DisplayInfo& display_info) {
  scoped_ptr<base::ListValue> js_resolutions(new base::ListValue);
  const std::vector<float> ui_scales =
      DisplayManager::GetScalesForDisplay(display_info);
  gfx::SizeF base_size = display_info.bounds_in_native().size();
  base_size.Scale(1.0f / display_info.device_scale_factor());
  if (display_info.rotation() == gfx::Display::ROTATE_90 ||
      display_info.rotation() == gfx::Display::ROTATE_270) {
    float tmp = base_size.width();
    base_size.set_width(base_size.height());
    base_size.set_height(tmp);
  }

  for (size_t i = 0; i < ui_scales.size(); ++i) {
    base::DictionaryValue* resolution_info = new base::DictionaryValue();
    gfx::SizeF new_size = base_size;
    new_size.Scale(ui_scales[i]);
    gfx::Size resolution = gfx::ToFlooredSize(new_size);
    resolution_info->SetDouble("scale", ui_scales[i]);
    if (ui_scales[i] == 1.0f)
      resolution_info->SetBoolean("isBest", true);
    resolution_info->SetBoolean(
        "selected", display_info.configured_ui_scale() == ui_scales[i]);
    resolution_info->SetInteger("width", resolution.width());
    resolution_info->SetInteger("height", resolution.height());
    js_resolutions->Append(resolution_info);
  }

  return js_resolutions.Pass();
}

scoped_ptr<base::ListValue> GetResolutionsForExternalDisplay(
    const ash::DisplayInfo& display_info) {
  scoped_ptr<base::ListValue> js_resolutions(new base::ListValue);

  gfx::Size current_size = display_info.bounds_in_native().size();
  gfx::Insets current_overscan = display_info.GetOverscanInsetsInPixel();
  int largest_index = -1;
  int largest_area = -1;

  for (size_t i = 0; i < display_info.display_modes().size(); ++i) {
    base::DictionaryValue* resolution_info = new base::DictionaryValue();
    const ash::DisplayMode& display_mode = display_info.display_modes()[i];
    gfx::Size resolution = display_mode.size;

    if (resolution.GetArea() > largest_area) {
      resolution_info->SetBoolean("isBest", true);
      largest_area = resolution.GetArea();
      if (largest_index >= 0) {
        base::DictionaryValue* prev_largest = NULL;
        CHECK(js_resolutions->GetDictionary(largest_index, &prev_largest));
        prev_largest->SetBoolean("isBest", false);
      }
      largest_index = i;
    }

    if (resolution == current_size) {
      // Right now, the scale factor for unselected resolutions is unknown.
      // TODO(mukai): Set the scale factor for unselected ones.
      resolution_info->SetDouble(
          "scaleFactor", display_info.device_scale_factor());
      resolution_info->SetBoolean("selected", true);
    }

    resolution.Enlarge(
        -current_overscan.width(), -current_overscan.height());
    resolution_info->SetInteger("width", resolution.width());
    resolution_info->SetInteger("height", resolution.height());
    resolution_info->SetDouble("refreshRate", display_mode.refresh_rate);
    js_resolutions->Append(resolution_info);
  }

  return js_resolutions.Pass();
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
      "setUIScale",
      base::Bind(&DisplayOptionsHandler::HandleSetUIScale,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setResolution",
      base::Bind(&DisplayOptionsHandler::HandleSetResolution,
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

    scoped_ptr<base::ListValue> js_resolutions = display.IsInternal() ?
        GetResolutionsForInternalDisplay(display_info) :
        GetResolutionsForExternalDisplay(display_info);
    std::sort(
        js_resolutions->begin(), js_resolutions->end(), CompareResolution);
    js_display->Set("resolutions", js_resolutions.release());

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
  ash::Shell::GetInstance()->output_configurator_animation()->
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
  ash::Shell::GetInstance()->output_configurator_animation()->
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
  ash::Shell::GetInstance()->output_configurator_animation()->
      StartFadeOutAnimation(base::Bind(
          &DisplayOptionsHandler::OnFadeOutForDisplayLayoutFinished,
          base::Unretained(this),
          static_cast<int>(layout),
          static_cast<int>(offset)));
}

void DisplayOptionsHandler::HandleSetUIScale(const base::ListValue* args) {
  DCHECK(!args->empty());

  int64 display_id = GetDisplayId(args);
  if (display_id == gfx::Display::kInvalidDisplayID)
    return;

  double ui_scale = 0.0f;
  if (!args->GetDouble(1, &ui_scale) || ui_scale == 0.0f) {
    LOG(ERROR) << "Can't find new ui_scale";
    return;
  }

  GetDisplayManager()->SetDisplayUIScale(display_id, ui_scale);
}

void DisplayOptionsHandler::HandleSetResolution(const base::ListValue* args) {
  DCHECK(!args->empty());
  int64 display_id = GetDisplayId(args);
  if (display_id == gfx::Display::kInvalidDisplayID)
    return;

  content::RecordAction(
      base::UserMetricsAction("Options_DisplaySetResolution"));
  double width = 0.0f;
  double height = 0.0f;
  if (!args->GetDouble(1, &width) || width == 0.0f) {
    LOG(ERROR) << "Can't find new width";
    return;
  }
  if (!args->GetDouble(2, &height) || height == 0.0f) {
    LOG(ERROR) << "Can't find new height";
    return;
  }

  const ash::DisplayInfo& display_info =
      GetDisplayManager()->GetDisplayInfo(display_id);
  gfx::Insets current_overscan = display_info.GetOverscanInsetsInPixel();
  gfx::Size new_resolution = gfx::ToFlooredSize(gfx::SizeF(width, height));
  new_resolution.Enlarge(current_overscan.width(), current_overscan.height());
  gfx::Size old_resolution = display_info.bounds_in_native().size();
  bool has_new_resolution = false;
  bool has_old_resolution = false;
  for (size_t i = 0; i < display_info.display_modes().size(); ++i) {
    ash::DisplayMode display_mode = display_info.display_modes()[i];
    if (display_mode.size == new_resolution)
      has_new_resolution = true;
    if (display_mode.size == old_resolution)
      has_old_resolution = true;
  }
  if (!has_new_resolution) {
    LOG(ERROR) << "No new resolution " << new_resolution.ToString()
               << " is found in the display info " << display_info.ToString();
    return;
  }
  if (!has_old_resolution) {
    LOG(ERROR) << "No old resolution " << old_resolution.ToString()
               << " is found in the display info " << display_info.ToString();
    return;
  }

  ash::Shell::GetInstance()->resolution_notification_controller()->
      SetDisplayResolutionAndNotify(
          display_id, old_resolution, new_resolution,
          base::Bind(&StoreDisplayPrefs));
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
