// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/display_options_handler.h"

#include <string>

#include "ash/display/display_controller.h"
#include "ash/display/output_configurator_animation.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/json/json_value_converter.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/display/output_configurator.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/aura/env.h"
#include "ui/aura/display_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"

namespace chromeos {
namespace options {

DisplayOptionsHandler::DisplayOptionsHandler() {
  aura::Env::GetInstance()->display_manager()->AddObserver(this);
}

DisplayOptionsHandler::~DisplayOptionsHandler() {
  aura::Env::GetInstance()->display_manager()->RemoveObserver(this);
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
  localized_strings->SetString("applyResult", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_APPLY_RESULT));
  localized_strings->SetString("resolution", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OPTIONS_RESOLUTION));
}

void DisplayOptionsHandler::InitializePage() {
  DCHECK(web_ui());
  UpdateDisplaySectionVisibility();
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
      "setDisplayLayout",
      base::Bind(&DisplayOptionsHandler::HandleDisplayLayout,
                 base::Unretained(this)));
}

void DisplayOptionsHandler::OnDisplayBoundsChanged(
    const gfx::Display& display) {
  SendDisplayInfo();
}

void DisplayOptionsHandler::OnDisplayAdded(const gfx::Display& new_display) {
  UpdateDisplaySectionVisibility();
  SendDisplayInfo();
}

void DisplayOptionsHandler::OnDisplayRemoved(const gfx::Display& old_display) {
  UpdateDisplaySectionVisibility();
  SendDisplayInfo();
}

void DisplayOptionsHandler::UpdateDisplaySectionVisibility() {
  chromeos::OutputState output_state =
      ash::Shell::GetInstance()->output_configurator()->output_state();
  base::FundamentalValue show_options(
      output_state != chromeos::STATE_INVALID &&
      output_state != chromeos::STATE_HEADLESS &&
      output_state != chromeos::STATE_SINGLE);
  web_ui()->CallJavascriptFunction(
      "options.BrowserOptions.showDisplayOptions", show_options);
}

void DisplayOptionsHandler::SendDisplayInfo() {
  aura::DisplayManager* display_manager =
      aura::Env::GetInstance()->display_manager();
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  chromeos::OutputConfigurator* output_configurator =
      ash::Shell::GetInstance()->output_configurator();
  base::FundamentalValue mirroring(
      output_configurator->output_state() == chromeos::STATE_DUAL_MIRROR);

  base::ListValue displays;
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    const gfx::Display* display = display_manager->GetDisplayAt(i);
    const gfx::Rect& bounds = display->bounds();
    base::DictionaryValue* js_display = new base::DictionaryValue();
    js_display->SetDouble("id", display->id());
    js_display->SetDouble("x", bounds.x());
    js_display->SetDouble("y", bounds.y());
    js_display->SetDouble("width", bounds.width());
    js_display->SetDouble("height", bounds.height());
    js_display->SetString("name", display_manager->GetDisplayNameAt(i));
    displays.Set(i, js_display);
  }

  scoped_ptr<base::Value> layout_value(base::Value::CreateNullValue());
  scoped_ptr<base::Value> offset_value(base::Value::CreateNullValue());
  if (display_manager->GetNumDisplays() > 1) {
    const std::string& secondary_display_name =
        display_manager->GetDisplayNameAt(1);
    const ash::DisplayLayout& layout =
        display_controller->GetLayoutForDisplayName(secondary_display_name);
    layout_value.reset(new base::FundamentalValue(layout.position));
    offset_value.reset(new base::FundamentalValue(layout.offset));
  }

  web_ui()->CallJavascriptFunction(
      "options.DisplayOptions.setDisplayInfo",
      mirroring, displays, *layout_value.get(), *offset_value.get());
}

void DisplayOptionsHandler::FadeOutForMirroringFinished(bool is_mirroring) {
  // We use 'PRIMARY_ONLY' for non-mirroring state for now.
  // TODO(mukai): fix this and support multiple display modes.
  chromeos::OutputState new_state =
      is_mirroring ? STATE_DUAL_MIRROR : STATE_DUAL_PRIMARY_ONLY;
  ash::Shell::GetInstance()->output_configurator()->SetDisplayMode(new_state);
  SendDisplayInfo();
  // Not necessary to start fade-in animation.  OutputConfigurator will do that.
}

void DisplayOptionsHandler::FadeOutForDisplayLayoutFinished(
    int layout, int offset) {
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  aura::DisplayManager* display_manager =
      aura::Env::GetInstance()->display_manager();
  // Assumes that there are two displays at most and the second item is the
  // secondary display.
  if (display_manager->GetNumDisplays() > 1) {
    gfx::Display* display = display_manager->GetDisplayAt(1);
    SetDisplayLayoutPref(pref_service, *display, layout, offset);
  }

  SendDisplayInfo();
  ash::Shell::GetInstance()->output_configurator_animation()->
      StartFadeInAnimation();
}

void DisplayOptionsHandler::HandleDisplayInfo(
    const base::ListValue* unused_args) {
  SendDisplayInfo();
}

void DisplayOptionsHandler::HandleMirroring(const base::ListValue* args) {
  DCHECK(!args->empty());
  bool is_mirroring = false;
  args->GetBoolean(0, &is_mirroring);
  ash::Shell::GetInstance()->output_configurator_animation()->
      StartFadeOutAnimation(base::Bind(
          &DisplayOptionsHandler::FadeOutForMirroringFinished,
          base::Unretained(this),
          is_mirroring));
}

void DisplayOptionsHandler::HandleDisplayLayout(const base::ListValue* args) {
  double layout = -1;
  double offset = -1;
  if (!args->GetDouble(0, &layout) || !args->GetDouble(1, &offset)) {
    LOG(ERROR) << "Invalid parameter";
    SendDisplayInfo();
    return;
  }
  DCHECK_LE(ash::DisplayLayout::TOP, layout);
  DCHECK_GE(ash::DisplayLayout::LEFT, layout);
  ash::Shell::GetInstance()->output_configurator_animation()->
      StartFadeOutAnimation(base::Bind(
          &DisplayOptionsHandler::FadeOutForDisplayLayoutFinished,
          base::Unretained(this),
          static_cast<int>(layout),
          static_cast<int>(offset)));
}

}  // namespace options
}  // namespace chromeos
