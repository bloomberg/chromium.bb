// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/display_options_handler.h"

#include <string>

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/display/output_configurator_animation.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chromeos/display/output_configurator.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/safe_integer_conversions.h"
#include "ui/gfx/screen.h"

using ash::internal::DisplayManager;

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

}  // namespace

DisplayOptionsHandler::DisplayOptionsHandler() {
  ash::Shell::GetInstance()->display_controller()->AddObserver(this);
}

DisplayOptionsHandler::~DisplayOptionsHandler() {
  ash::Shell::GetInstance()->display_controller()->RemoveObserver(this);
}

void DisplayOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
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
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_ORIENTATION_90));
  localized_strings->SetString("orientation180", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_ORIENTATION_180));
  localized_strings->SetString("orientation270", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_ORIENTATION_270));
  localized_strings->SetString(
      "startCalibratingOverscan", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_START_CALIBRATING_OVERSCAN));
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
      "setOrientation",
      base::Bind(&DisplayOptionsHandler::HandleSetOrientation,
                 base::Unretained(this)));
}

void DisplayOptionsHandler::OnDisplayConfigurationChanging() {
}

void DisplayOptionsHandler::OnDisplayConfigurationChanged() {
  SendAllDisplayInfo();
}

void DisplayOptionsHandler::SendAllDisplayInfo() {
  DisplayManager* display_manager = GetDisplayManager();

  std::vector<const gfx::Display*> displays;
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    displays.push_back(display_manager->GetDisplayAt(i));
  }
  SendDisplayInfo(displays);
}

void DisplayOptionsHandler::SendDisplayInfo(
    const std::vector<const gfx::Display*> displays) {
  DisplayManager* display_manager = GetDisplayManager();
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  base::FundamentalValue mirroring(display_manager->IsMirrored());

  int64 primary_id = ash::Shell::GetScreen()->GetPrimaryDisplay().id();
  base::ListValue js_displays;
  for (size_t i = 0; i < displays.size(); ++i) {
    const gfx::Display* display = displays[i];
    const ash::internal::DisplayInfo& display_info =
        display_manager->GetDisplayInfo(display->id());
    const gfx::Rect& bounds = display->bounds();
    base::DictionaryValue* js_display = new base::DictionaryValue();
    js_display->SetString("id", base::Int64ToString(display->id()));
    js_display->SetInteger("x", bounds.x());
    js_display->SetInteger("y", bounds.y());
    js_display->SetInteger("width", bounds.width());
    js_display->SetInteger("height", bounds.height());
    js_display->SetString("name",
                          display_manager->GetDisplayNameForId(display->id()));
    js_display->SetBoolean("isPrimary", display->id() == primary_id);
    js_display->SetBoolean("isInternal", display->IsInternal());
    js_display->SetInteger("orientation",
                           static_cast<int>(display_info.rotation()));
    std::vector<float> ui_scales = DisplayManager::GetScalesForDisplay(
        display_info);
    base::ListValue* js_scales = new base::ListValue();
    gfx::SizeF base_size = display_info.bounds_in_pixel().size();
    base_size.Scale(1.0f / display->device_scale_factor());
    if (display_info.rotation() == gfx::Display::ROTATE_90 ||
        display_info.rotation() == gfx::Display::ROTATE_270) {
      float tmp = base_size.width();
      base_size.set_width(base_size.height());
      base_size.set_height(tmp);
    }

    for (size_t i = 0; i < ui_scales.size(); ++i) {
      base::DictionaryValue* scale_info = new base::DictionaryValue();
      scale_info->SetDouble("scale", ui_scales[i]);
      scale_info->SetInteger(
          "width", gfx::ToFlooredInt(base_size.width() * ui_scales[i]));
      scale_info->SetInteger(
          "height", gfx::ToFlooredInt(base_size.height() * ui_scales[i]));
      scale_info->SetBoolean("selected",
                             display_info.ui_scale() == ui_scales[i]);
      js_scales->Append(scale_info);
    }
    js_display->Set("uiScales", js_scales);
    js_displays.Append(js_display);
  }

  scoped_ptr<base::Value> layout_value(base::Value::CreateNullValue());
  scoped_ptr<base::Value> offset_value(base::Value::CreateNullValue());
  if (display_manager->GetNumDisplays() > 1) {
    const ash::DisplayLayout layout =
        display_controller->GetCurrentDisplayLayout();
    layout_value.reset(new base::FundamentalValue(layout.position));
    offset_value.reset(new base::FundamentalValue(layout.offset));
  }

  web_ui()->CallJavascriptFunction(
      "options.DisplayOptions.setDisplayInfo",
      mirroring, js_displays, *layout_value.get(), *offset_value.get());
}

void DisplayOptionsHandler::OnFadeOutForMirroringFinished(bool is_mirroring) {
  ash::Shell::GetInstance()->display_manager()->SetMirrorMode(is_mirroring);
  // Not necessary to start fade-in animation.  OutputConfigurator will do that.
}

void DisplayOptionsHandler::OnFadeOutForDisplayLayoutFinished(
    int position, int offset) {
  SetCurrentAndDefaultDisplayLayout(
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

  std::string ui_scale_value;
  double ui_scale = 0.0f;
  if (!args->GetString(1, &ui_scale_value)) {
    LOG(ERROR) << "Ca't find new ui_scale";
    return;
  }
  if (!base::StringToDouble(ui_scale_value, &ui_scale)) {
    LOG(ERROR) << "Invalid ui_scale: " << ui_scale_value;
    return;
  }

  GetDisplayManager()->SetDisplayUIScale(display_id, ui_scale);
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

  GetDisplayManager()->SetDisplayRotation(display_id, new_rotation);
}

}  // namespace options
}  // namespace chromeos
