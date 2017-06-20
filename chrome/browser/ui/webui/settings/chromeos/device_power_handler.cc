// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_power_handler.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

base::string16 GetBatteryTimeText(base::TimeDelta time_left) {
  int hour = 0;
  int min = 0;
  ash::PowerStatus::SplitTimeIntoHoursAndMinutes(time_left, &hour, &min);

  base::string16 time_text;
  if (hour == 0 || min == 0) {
    // Display only one unit ("2 hours" or "10 minutes").
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG, time_left);
  }

  return ui::TimeFormat::Detailed(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG,
                                  -1,  // force hour and minute output
                                  time_left);
}

}  // namespace

const char PowerHandler::kPowerManagementSettingsChangedName[] =
    "power-management-settings-changed";
const char PowerHandler::kIdleBehaviorKey[] = "idleBehavior";
const char PowerHandler::kIdleControlledKey[] = "idleControlled";
const char PowerHandler::kLidClosedBehaviorKey[] = "lidClosedBehavior";
const char PowerHandler::kLidClosedControlledKey[] = "lidClosedControlled";
const char PowerHandler::kHasLidKey[] = "hasLid";

PowerHandler::TestAPI::TestAPI(PowerHandler* handler) : handler_(handler) {}

PowerHandler::TestAPI::~TestAPI() = default;

void PowerHandler::TestAPI::RequestPowerManagementSettings() {
  base::ListValue args;
  handler_->HandleRequestPowerManagementSettings(&args);
}

void PowerHandler::TestAPI::SetIdleBehavior(IdleBehavior behavior) {
  base::ListValue args;
  args.AppendInteger(static_cast<int>(behavior));
  handler_->HandleSetIdleBehavior(&args);
}

void PowerHandler::TestAPI::SetLidClosedBehavior(
    PowerPolicyController::Action behavior) {
  base::ListValue args;
  args.AppendInteger(behavior);
  handler_->HandleSetLidClosedBehavior(&args);
}

PowerHandler::PowerHandler(PrefService* prefs)
    : prefs_(prefs),
      power_status_observer_(this),
      power_manager_client_observer_(this),
      weak_ptr_factory_(this) {
  power_status_ = ash::PowerStatus::Get();
}

PowerHandler::~PowerHandler() {}

void PowerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "updatePowerStatus", base::Bind(&PowerHandler::HandleUpdatePowerStatus,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setPowerSource",
      base::Bind(&PowerHandler::HandleSetPowerSource, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestPowerManagementSettings",
      base::Bind(&PowerHandler::HandleRequestPowerManagementSettings,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setLidClosedBehavior",
      base::Bind(&PowerHandler::HandleSetLidClosedBehavior,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setIdleBehavior",
      base::Bind(&PowerHandler::HandleSetIdleBehavior, base::Unretained(this)));
}

void PowerHandler::OnJavascriptAllowed() {
  power_status_observer_.Add(power_status_);

  PowerManagerClient* power_manager_client =
      DBusThreadManager::Get()->GetPowerManagerClient();
  power_manager_client_observer_.Add(power_manager_client);
  power_manager_client->GetSwitchStates(base::Bind(
      &PowerHandler::OnGotSwitchStates, weak_ptr_factory_.GetWeakPtr()));

  // Observe power management prefs used in the UI.
  base::Closure callback(base::Bind(&PowerHandler::SendPowerManagementSettings,
                                    base::Unretained(this), false /* force */));
  pref_change_registrar_ = base::MakeUnique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs_);
  pref_change_registrar_->Add(prefs::kPowerAcIdleAction, callback);
  pref_change_registrar_->Add(prefs::kPowerAcScreenDimDelayMs, callback);
  pref_change_registrar_->Add(prefs::kPowerAcScreenOffDelayMs, callback);
  pref_change_registrar_->Add(prefs::kPowerAcScreenLockDelayMs, callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryIdleAction, callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryScreenDimDelayMs, callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryScreenOffDelayMs, callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryScreenLockDelayMs, callback);
  pref_change_registrar_->Add(prefs::kPowerLidClosedAction, callback);
}

void PowerHandler::OnJavascriptDisallowed() {
  power_status_observer_.RemoveAll();
  power_manager_client_observer_.RemoveAll();
  pref_change_registrar_.reset();
}

void PowerHandler::OnPowerStatusChanged() {
  SendBatteryStatus();
  SendPowerSources();
}

void PowerHandler::PowerManagerRestarted() {
  DBusThreadManager::Get()->GetPowerManagerClient()->GetSwitchStates(base::Bind(
      &PowerHandler::OnGotSwitchStates, weak_ptr_factory_.GetWeakPtr()));
}

void PowerHandler::LidEventReceived(PowerManagerClient::LidState state,
                                    const base::TimeTicks& timestamp) {
  lid_state_ = state;
  SendPowerManagementSettings(false /* force */);
}

void PowerHandler::HandleUpdatePowerStatus(const base::ListValue* args) {
  AllowJavascript();
  power_status_->RequestStatusUpdate();
}

void PowerHandler::HandleSetPowerSource(const base::ListValue* args) {
  AllowJavascript();

  std::string id;
  CHECK(args->GetString(0, &id));
  power_status_->SetPowerSource(id);
}

void PowerHandler::HandleRequestPowerManagementSettings(
    const base::ListValue* args) {
  AllowJavascript();
  SendPowerManagementSettings(true /* force */);
}

void PowerHandler::HandleSetIdleBehavior(const base::ListValue* args) {
  AllowJavascript();

  int value = 0;
  CHECK(args->GetInteger(0, &value));
  switch (static_cast<IdleBehavior>(value)) {
    case IdleBehavior::DISPLAY_OFF_SLEEP:
      // The default behavior is to turn the display off and sleep. Clear the
      // prefs so we use the default delays.
      prefs_->ClearPref(prefs::kPowerAcIdleAction);
      prefs_->ClearPref(prefs::kPowerAcScreenDimDelayMs);
      prefs_->ClearPref(prefs::kPowerAcScreenOffDelayMs);
      prefs_->ClearPref(prefs::kPowerAcScreenLockDelayMs);
      prefs_->ClearPref(prefs::kPowerBatteryIdleAction);
      prefs_->ClearPref(prefs::kPowerBatteryScreenDimDelayMs);
      prefs_->ClearPref(prefs::kPowerBatteryScreenOffDelayMs);
      prefs_->ClearPref(prefs::kPowerBatteryScreenLockDelayMs);
      break;
    case IdleBehavior::DISPLAY_OFF_STAY_AWAKE:
      // Override idle actions to keep the system awake, but use the default
      // screen delays.
      prefs_->SetInteger(prefs::kPowerAcIdleAction,
                         PowerPolicyController::ACTION_DO_NOTHING);
      prefs_->ClearPref(prefs::kPowerAcScreenDimDelayMs);
      prefs_->ClearPref(prefs::kPowerAcScreenOffDelayMs);
      prefs_->ClearPref(prefs::kPowerAcScreenLockDelayMs);
      prefs_->SetInteger(prefs::kPowerBatteryIdleAction,
                         PowerPolicyController::ACTION_DO_NOTHING);
      prefs_->ClearPref(prefs::kPowerBatteryScreenDimDelayMs);
      prefs_->ClearPref(prefs::kPowerBatteryScreenOffDelayMs);
      prefs_->ClearPref(prefs::kPowerBatteryScreenLockDelayMs);
      break;
    case IdleBehavior::DISPLAY_ON:
      // Override idle actions and set screen delays to 0 in order to disable
      // them (i.e. keep the screen on).
      prefs_->SetInteger(prefs::kPowerAcIdleAction,
                         PowerPolicyController::ACTION_DO_NOTHING);
      prefs_->SetInteger(prefs::kPowerAcScreenDimDelayMs, 0);
      prefs_->SetInteger(prefs::kPowerAcScreenOffDelayMs, 0);
      prefs_->SetInteger(prefs::kPowerAcScreenLockDelayMs, 0);
      prefs_->SetInteger(prefs::kPowerBatteryIdleAction,
                         PowerPolicyController::ACTION_DO_NOTHING);
      prefs_->SetInteger(prefs::kPowerBatteryScreenDimDelayMs, 0);
      prefs_->SetInteger(prefs::kPowerBatteryScreenOffDelayMs, 0);
      prefs_->SetInteger(prefs::kPowerBatteryScreenLockDelayMs, 0);
      break;
    default:
      NOTREACHED() << "Invalid idle behavior " << value;
  }
}

void PowerHandler::HandleSetLidClosedBehavior(const base::ListValue* args) {
  AllowJavascript();

  int value = 0;
  CHECK(args->GetInteger(0, &value));
  switch (static_cast<PowerPolicyController::Action>(value)) {
    case PowerPolicyController::ACTION_SUSPEND:
      prefs_->ClearPref(prefs::kPowerLidClosedAction);
      break;
    case PowerPolicyController::ACTION_DO_NOTHING:
      prefs_->SetInteger(prefs::kPowerLidClosedAction,
                         PowerPolicyController::ACTION_DO_NOTHING);
      break;
    default:
      NOTREACHED() << "Unsupported lid-closed behavior " << value;
  }
}

void PowerHandler::SendBatteryStatus() {
  bool charging = power_status_->IsBatteryCharging();
  bool calculating = power_status_->IsBatteryTimeBeingCalculated();
  int percent = power_status_->GetRoundedBatteryPercent();
  base::TimeDelta time_left;
  bool show_time = false;

  if (!calculating) {
    time_left = charging ? power_status_->GetBatteryTimeToFull()
                         : power_status_->GetBatteryTimeToEmpty();
    show_time = ash::PowerStatus::ShouldDisplayBatteryTime(time_left);
  }

  base::string16 status_text;
  if (show_time) {
    status_text = l10n_util::GetStringFUTF16(
        charging ? IDS_OPTIONS_BATTERY_STATUS_CHARGING
                 : IDS_OPTIONS_BATTERY_STATUS,
        base::IntToString16(percent), GetBatteryTimeText(time_left));
  } else {
    status_text = l10n_util::GetStringFUTF16(IDS_OPTIONS_BATTERY_STATUS_SHORT,
                                             base::IntToString16(percent));
  }

  base::DictionaryValue battery_dict;
  battery_dict.SetBoolean("charging", charging);
  battery_dict.SetBoolean("calculating", calculating);
  battery_dict.SetInteger("percent", percent);
  battery_dict.SetString("statusText", status_text);

  FireWebUIListener("battery-status-changed", battery_dict);
}

void PowerHandler::SendPowerSources() {
  base::ListValue sources_list;
  for (const auto& source : power_status_->GetPowerSources()) {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("id", source.id);
    dict->SetInteger("type", source.type);
    dict->SetString("description",
                    l10n_util::GetStringUTF16(source.description_id));
    sources_list.Append(std::move(dict));
  }

  FireWebUIListener("power-sources-changed", sources_list,
                    base::Value(power_status_->GetCurrentPowerSourceID()),
                    base::Value(power_status_->IsUsbChargerConnected()));
}

void PowerHandler::SendPowerManagementSettings(bool force) {
  // Infer the idle behavior based on the idle action (determining whether we'll
  // sleep eventually or not) and the AC screen-off delay. Policy can request
  // more-nuanced combinations of AC/battery actions and delays, but we wouldn't
  // be able to display something meaningful in the UI in those cases anyway.
  const PowerPolicyController::Action idle_action =
      static_cast<PowerPolicyController::Action>(
          prefs_->GetInteger(prefs::kPowerAcIdleAction));
  IdleBehavior idle_behavior = IdleBehavior::OTHER;
  if (idle_action == PowerPolicyController::ACTION_SUSPEND) {
    idle_behavior = IdleBehavior::DISPLAY_OFF_SLEEP;
  } else if (idle_action == PowerPolicyController::ACTION_DO_NOTHING) {
    idle_behavior = (prefs_->GetInteger(prefs::kPowerAcScreenOffDelayMs) > 0
                         ? IdleBehavior::DISPLAY_OFF_STAY_AWAKE
                         : IdleBehavior::DISPLAY_ON);
  }

  const bool idle_controlled =
      prefs_->IsManagedPreference(prefs::kPowerAcIdleAction) ||
      prefs_->IsManagedPreference(prefs::kPowerAcScreenDimDelayMs) ||
      prefs_->IsManagedPreference(prefs::kPowerAcScreenOffDelayMs) ||
      prefs_->IsManagedPreference(prefs::kPowerAcScreenLockDelayMs) ||
      prefs_->IsManagedPreference(prefs::kPowerBatteryIdleAction) ||
      prefs_->IsManagedPreference(prefs::kPowerBatteryScreenDimDelayMs) ||
      prefs_->IsManagedPreference(prefs::kPowerBatteryScreenOffDelayMs) ||
      prefs_->IsManagedPreference(prefs::kPowerBatteryScreenLockDelayMs);

  const PowerPolicyController::Action lid_closed_behavior =
      static_cast<PowerPolicyController::Action>(
          prefs_->GetInteger(prefs::kPowerLidClosedAction));
  const bool lid_closed_controlled =
      prefs_->IsManagedPreference(prefs::kPowerLidClosedAction);
  const bool has_lid = lid_state_ != PowerManagerClient::LidState::NOT_PRESENT;

  // Don't notify the UI if nothing changed.
  if (!force && idle_behavior == last_idle_behavior_ &&
      idle_controlled == last_idle_controlled_ &&
      lid_closed_behavior == last_lid_closed_behavior_ &&
      lid_closed_controlled == last_lid_closed_controlled_ &&
      has_lid == last_has_lid_)
    return;

  base::DictionaryValue dict;
  dict.SetInteger(kIdleBehaviorKey, static_cast<int>(idle_behavior));
  dict.SetBoolean(kIdleControlledKey, idle_controlled);
  dict.SetInteger(kLidClosedBehaviorKey, lid_closed_behavior);
  dict.SetBoolean(kLidClosedControlledKey, lid_closed_controlled);
  dict.SetBoolean(kHasLidKey, has_lid);
  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::Value(kPowerManagementSettingsChangedName),
                         dict);

  last_idle_behavior_ = idle_behavior;
  last_idle_controlled_ = idle_controlled;
  last_lid_closed_behavior_ = lid_closed_behavior;
  last_lid_closed_controlled_ = lid_closed_controlled;
  last_has_lid_ = has_lid;
}

void PowerHandler::OnGotSwitchStates(
    PowerManagerClient::LidState lid_state,
    PowerManagerClient::TabletMode tablet_mode) {
  lid_state_ = lid_state;
  SendPowerManagementSettings(false /* force */);
}

}  // namespace settings
}  // namespace chromeos
