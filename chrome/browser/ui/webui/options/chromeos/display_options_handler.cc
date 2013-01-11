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
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chrome/browser/chromeos/display/overscan_calibrator.h"
#include "chrome/common/pref_names.h"
#include "chromeos/display/output_configurator.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

using ash::internal::DisplayManager;

namespace chromeos {
namespace options {
namespace {

DisplayManager* GetDisplayManager() {
  return ash::Shell::GetInstance()->display_manager();
}

}  // namespace

DisplayOptionsHandler::DisplayOptionsHandler() {
  ash::Shell::GetScreen()->AddObserver(this);
}

DisplayOptionsHandler::~DisplayOptionsHandler() {
  ash::Shell::GetScreen()->RemoveObserver(this);
}

void DisplayOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  RegisterTitle(localized_strings, "displayOptionsPage",
                IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_TAB_TITLE);
  localized_strings->SetString("startMirroring", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_START_MIRRORING));
  localized_strings->SetString("stopMirroring", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_STOP_MIRRORING));
  localized_strings->SetString("setPrimary", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_SET_PRIMARY));
  localized_strings->SetString("applyResult", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_APPLY_RESULT));
  localized_strings->SetString("resolution", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_RESOLUTION));
  localized_strings->SetString(
      "startCalibratingOverscan", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_START_CALIBRATING_OVERSCAN));
  localized_strings->SetString(
      "finishCalibratingOverscan", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_FINISH_CALIBRATING_OVERSCAN));
  localized_strings->SetString(
      "clearCalibratingOverscan", l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_CLEAR_CALIBRATING_OVERSCAN));
}

void DisplayOptionsHandler::InitializePage() {
  DCHECK(web_ui());
  UpdateDisplaySectionVisibility(GetDisplayManager()->GetNumDisplays());
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
      "startOverscanCalibration",
      base::Bind(&DisplayOptionsHandler::HandleStartOverscanCalibration,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "finishOverscanCalibration",
      base::Bind(&DisplayOptionsHandler::HandleFinishOverscanCalibration,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearOverscanCalibration",
      base::Bind(&DisplayOptionsHandler::HandleClearOverscanCalibration,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateOverscanCalibration",
      base::Bind(&DisplayOptionsHandler::HandleUpdateOverscanCalibration,
                 base::Unretained(this)));
}

void DisplayOptionsHandler::OnDisplayBoundsChanged(
    const gfx::Display& display) {
}

void DisplayOptionsHandler::OnDisplayAdded(const gfx::Display& new_display) {
  UpdateDisplaySectionVisibility(GetDisplayManager()->GetNumDisplays());
  SendAllDisplayInfo();
}

void DisplayOptionsHandler::OnDisplayRemoved(const gfx::Display& old_display) {
  DisplayManager* display_manager = GetDisplayManager();
  UpdateDisplaySectionVisibility(display_manager->GetNumDisplays() - 1);

  std::vector<const gfx::Display*> displays;
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    const gfx::Display* display = display_manager->GetDisplayAt(i);
    if (display->id() != old_display.id())
      displays.push_back(display);
  }
  SendDisplayInfo(displays);
}

void DisplayOptionsHandler::UpdateDisplaySectionVisibility(
    size_t num_displays) {
  chromeos::OutputState output_state =
      ash::Shell::GetInstance()->output_configurator()->output_state();
  base::FundamentalValue show_options(
      num_displays > 1 || output_state == chromeos::STATE_DUAL_MIRROR);
  web_ui()->CallJavascriptFunction(
      "options.BrowserOptions.showDisplayOptions", show_options);
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
  chromeos::OutputConfigurator* output_configurator =
      ash::Shell::GetInstance()->output_configurator();
  base::FundamentalValue mirroring(
      output_configurator->output_state() == chromeos::STATE_DUAL_MIRROR);

  int64 primary_id = ash::Shell::GetScreen()->GetPrimaryDisplay().id();
  base::ListValue display_info;
  for (size_t i = 0; i < displays.size(); ++i) {
    const gfx::Display* display = displays[i];
    const gfx::Rect& bounds = display->bounds();
    base::DictionaryValue* js_display = new base::DictionaryValue();
    js_display->SetString("id", base::Int64ToString(display->id()));
    js_display->SetDouble("x", bounds.x());
    js_display->SetDouble("y", bounds.y());
    js_display->SetDouble("width", bounds.width());
    js_display->SetDouble("height", bounds.height());
    js_display->SetString("name",
                          display_manager->GetDisplayNameFor(*display));
    js_display->SetBoolean("isPrimary", display->id() == primary_id);
    js_display->SetBoolean("isInternal",
                           display_manager->IsInternalDisplayId(display->id()));
    base::DictionaryValue* js_insets = new base::DictionaryValue();
    const gfx::Insets& insets =
        display_controller->GetOverscanInsets(display->id());
    js_insets->SetInteger("top", insets.top());
    js_insets->SetInteger("left", insets.left());
    js_insets->SetInteger("bottom", insets.bottom());
    js_insets->SetInteger("right", insets.right());
    js_display->Set("overscan", js_insets);
    display_info.Set(i, js_display);
  }

  scoped_ptr<base::Value> layout_value(base::Value::CreateNullValue());
  scoped_ptr<base::Value> offset_value(base::Value::CreateNullValue());
  if (display_manager->GetNumDisplays() > 1) {
    const gfx::Display secondary_display =
        ash::ScreenAsh::GetSecondaryDisplay();
    const ash::DisplayLayout& layout =
        display_controller->GetLayoutForDisplay(secondary_display);
    layout_value.reset(new base::FundamentalValue(layout.position));
    offset_value.reset(new base::FundamentalValue(layout.offset));
  }

  web_ui()->CallJavascriptFunction(
      "options.DisplayOptions.setDisplayInfo",
      mirroring, display_info, *layout_value.get(), *offset_value.get());
}

void DisplayOptionsHandler::OnFadeOutForMirroringFinished(bool is_mirroring) {
  // We use 'PRIMARY_ONLY' for non-mirroring state for now.
  // TODO(mukai): fix this and support multiple display modes.
  chromeos::OutputState new_state =
      is_mirroring ? STATE_DUAL_MIRROR : STATE_DUAL_PRIMARY_ONLY;
  ash::Shell::GetInstance()->output_configurator()->SetDisplayMode(new_state);
  SendAllDisplayInfo();
  // Not necessary to start fade-in animation.  OutputConfigurator will do that.
}

void DisplayOptionsHandler::OnFadeOutForDisplayLayoutFinished(
    int layout, int offset) {
  const gfx::Display& secondary_display = ash::ScreenAsh::GetSecondaryDisplay();
  if (secondary_display.is_valid())
    SetDisplayLayoutPref(secondary_display, layout, offset);

  SendAllDisplayInfo();
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

  int64 display_id = gfx::Display::kInvalidDisplayID;
  std::string id_value;
  if (!args->GetString(0, &id_value)) {
    LOG(ERROR) << "Can't find ID";
    return;
  }

  if (!base::StringToInt64(id_value, &display_id) ||
      display_id == gfx::Display::kInvalidDisplayID) {
    LOG(ERROR) << "Invalid parameter: " << id_value;
    return;
  }

  SetPrimaryDisplayIDPref(display_id);
  SendAllDisplayInfo();
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

void DisplayOptionsHandler::HandleStartOverscanCalibration(
    const base::ListValue* args) {
  int64 display_id = gfx::Display::kInvalidDisplayID;
  std::string id_value;
  if (!args->GetString(0, &id_value)) {
    LOG(ERROR) << "Can't find ID";
    return;
  }
  if (!base::StringToInt64(id_value, &display_id) ||
      display_id == gfx::Display::kInvalidDisplayID) {
    LOG(ERROR) << "Invalid parameter: " << id_value;
    return;
  }

  const gfx::Display& display = ash::ScreenAsh::GetDisplayForId(display_id);
  DCHECK(display.is_valid());
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  overscan_calibrator_.reset(new OverscanCalibrator(
      display,
      display_controller->GetOverscanInsets(display_id)));
}

void DisplayOptionsHandler::HandleFinishOverscanCalibration(
    const base::ListValue* args) {
  if (overscan_calibrator_.get()) {
    overscan_calibrator_->Commit();
    overscan_calibrator_.reset();
  }
  SendAllDisplayInfo();
}

void DisplayOptionsHandler::HandleClearOverscanCalibration(
    const base::ListValue* args) {
  if (overscan_calibrator_.get()) {
    overscan_calibrator_->UpdateInsets(gfx::Insets());
    overscan_calibrator_->Commit();
  }
  SendAllDisplayInfo();
}

void DisplayOptionsHandler::HandleUpdateOverscanCalibration(
    const base::ListValue* args) {
  if (!overscan_calibrator_.get())
    return;

  double top = 0, left = 0, bottom = 0, right = 0;
  if (!args->GetDouble(0, &top) || !args->GetDouble(1, &left) ||
      !args->GetDouble(2, &bottom) || !args->GetDouble(3, &right)) {
    LOG(ERROR) << "Can't find overscan insets data.";
    return;
  }

  overscan_calibrator_->UpdateInsets(gfx::Insets(top, left, bottom, right));
}

}  // namespace options
}  // namespace chromeos
