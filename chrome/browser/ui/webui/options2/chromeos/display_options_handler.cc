// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/display_options_handler.h"

#include <string>

#include "ash/monitor/monitor_controller.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/json/json_value_converter.h"
#include "base/values.h"
#include "chromeos/monitor/output_configurator.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/aura/env.h"
#include "ui/aura/monitor_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"

namespace chromeos {
namespace options2 {

using ash::internal::MonitorController;

DisplayOptionsHandler::DisplayOptionsHandler() {
  aura::Env::GetInstance()->monitor_manager()->AddObserver(this);
}

DisplayOptionsHandler::~DisplayOptionsHandler() {
  aura::Env::GetInstance()->monitor_manager()->RemoveObserver(this);
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
}

void DisplayOptionsHandler::InitializeHandler() {
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
  aura::MonitorManager* monitor_manager =
      aura::Env::GetInstance()->monitor_manager();
  chromeos::State output_state =
      ash::Shell::GetInstance()->output_configurator()->output_state();
  base::FundamentalValue show_options(
      MonitorController::IsExtendedDesktopEnabled() &&
      monitor_manager->GetNumDisplays() > 1 &&
      output_state != chromeos::STATE_INVALID &&
      output_state != chromeos::STATE_HEADLESS &&
      output_state != chromeos::STATE_SINGLE);
  web_ui()->CallJavascriptFunction(
      "options.BrowserOptions.showDisplayOptions", show_options);
}

void DisplayOptionsHandler::SendDisplayInfo() {
  aura::MonitorManager* monitor_manager =
      aura::Env::GetInstance()->monitor_manager();
  chromeos::OutputConfigurator* output_configurator =
      ash::Shell::GetInstance()->output_configurator();
  base::FundamentalValue mirroring(
      output_configurator->output_state() == chromeos::STATE_DUAL_MIRROR);

  base::ListValue displays;
  for (size_t i = 0; i < monitor_manager->GetNumDisplays(); ++i) {
    const gfx::Display& display = monitor_manager->GetDisplayAt(i);
    const gfx::Rect& bounds = display.bounds();
    base::DictionaryValue* js_display = new base::DictionaryValue();
    js_display->SetDouble("id", display.id());
    js_display->SetDouble("x", bounds.x());
    js_display->SetDouble("y", bounds.y());
    js_display->SetDouble("width", bounds.width());
    js_display->SetDouble("height", bounds.height());
    displays.Set(i, js_display);
  }

  MonitorController* monitor_controller =
      ash::Shell::GetInstance()->monitor_controller();
  base::FundamentalValue layout(static_cast<int>(
      monitor_controller->secondary_display_layout()));

  web_ui()->CallJavascriptFunction(
      "options.DisplayOptions.setDisplayInfo",
      mirroring, displays, layout);
}

void DisplayOptionsHandler::HandleDisplayInfo(
    const base::ListValue* unused_args) {
  SendDisplayInfo();
}

void DisplayOptionsHandler::HandleMirroring(const base::ListValue* args) {
  DCHECK(!args->empty());
  bool is_mirroring = false;
  args->GetBoolean(0, &is_mirroring);
  // We use 'PRIMARY_ONLY' for non-mirroring state for now.
  // TODO(mukai): fix this and support multiple display modes.
  chromeos::State new_state =
      is_mirroring ? STATE_DUAL_MIRROR : STATE_DUAL_PRIMARY_ONLY;
  ash::Shell::GetInstance()->output_configurator()->SetDisplayMode(new_state);
  SendDisplayInfo();
}

void DisplayOptionsHandler::HandleDisplayLayout(const base::ListValue* args) {
  double layout = -1;
  if (!args->GetDouble(0, &layout)) {
    LOG(ERROR) << "Invalid parameter";
    return;
  }
  DCHECK_LE(MonitorController::TOP, layout);
  DCHECK_GE(MonitorController::LEFT, layout);

  ash::Shell::GetInstance()->monitor_controller()->SetSecondaryDisplayLayout(
      static_cast<MonitorController::SecondaryDisplayLayout>(layout));
  SendDisplayInfo();
}

}  // namespace options2
}  // namespace chromeos
