// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/power_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "grit/ash_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

using ash::PowerStatus;

namespace chromeos {
namespace {

// Returns the message ID corresponding to the port location of a source.
int GetPowerSourceDescription(const PowerStatus::PowerSource& source) {
  switch (source.port) {
    case PowerStatus::UNKNOWN_PORT:
      return IDS_OPTIONS_POWER_SOURCE_PORT_UNKNOWN;
    case PowerStatus::LEFT_PORT:
      return IDS_OPTIONS_POWER_SOURCE_PORT_LEFT;
    case PowerStatus::RIGHT_PORT:
      return IDS_OPTIONS_POWER_SOURCE_PORT_RIGHT;
    case PowerStatus::BACK_PORT:
      return IDS_OPTIONS_POWER_SOURCE_PORT_BACK;
    case PowerStatus::FRONT_PORT:
      return IDS_OPTIONS_POWER_SOURCE_PORT_FRONT;
    case PowerStatus::LEFT_FRONT_PORT:
      return IDS_OPTIONS_POWER_SOURCE_PORT_LEFT_FRONT;
    case PowerStatus::LEFT_BACK_PORT:
      return IDS_OPTIONS_POWER_SOURCE_PORT_LEFT_BACK;
    case PowerStatus::RIGHT_FRONT_PORT:
      return IDS_OPTIONS_POWER_SOURCE_PORT_RIGHT_FRONT;
    case PowerStatus::RIGHT_BACK_PORT:
      return IDS_OPTIONS_POWER_SOURCE_PORT_RIGHT_BACK;
    case PowerStatus::BACK_LEFT_PORT:
      return IDS_OPTIONS_POWER_SOURCE_PORT_BACK_LEFT;
    case PowerStatus::BACK_RIGHT_PORT:
      return IDS_OPTIONS_POWER_SOURCE_PORT_BACK_RIGHT;
  }
  NOTREACHED();
  return IDS_OPTIONS_POWER_SOURCE_PORT_UNKNOWN;
}

}  // namespace

namespace options {

PowerHandler::PowerHandler() {
  this->show_power_status_ = switches::PowerOverlayEnabled() ||
                             (PowerStatus::Get()->IsBatteryPresent() &&
                              PowerStatus::Get()->SupportsDualRoleDevices());
}

PowerHandler::~PowerHandler() {
  if (this->show_power_status_)
    PowerStatus::Get()->RemoveObserver(this);
}

void PowerHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  RegisterTitle(localized_strings, "powerOverlay",
                IDS_OPTIONS_POWER_OVERLAY_TITLE);
  localized_strings->SetString(
      "batteryStatusLabel",
      l10n_util::GetStringUTF16(IDS_OPTIONS_BATTERY_STATUS_LABEL));
  localized_strings->SetBoolean(
      "showPowerStatus", this->show_power_status_);
  localized_strings->SetString(
      "powerSourceLabel",
      l10n_util::GetStringUTF16(IDS_OPTIONS_POWER_SOURCE_LABEL));
  localized_strings->SetString(
      "powerSourceBattery",
      l10n_util::GetStringUTF16(IDS_OPTIONS_POWER_SOURCE_BATTERY));
  localized_strings->SetString(
      "powerSourceAcAdapter",
      l10n_util::GetStringUTF16(IDS_OPTIONS_POWER_SOURCE_AC_ADAPTER));
  localized_strings->SetString(
      "powerSourceLowPowerCharger",
      l10n_util::GetStringUTF16(IDS_OPTIONS_POWER_SOURCE_LOW_POWER_CHARGER));
  localized_strings->SetString(
      "chargingDevices",
      l10n_util::GetStringUTF16(IDS_OPTIONS_POWER_OVERLAY_CHARGING_DEVICES));
  localized_strings->SetString(
      "calculatingPower",
      l10n_util::GetStringUTF16(IDS_OPTIONS_POWER_OVERLAY_CALCULATING));

  localized_strings->SetInteger("powerSourceCharger",
                                PowerStatus::DEDICATED_CHARGER);
  localized_strings->SetInteger("powerSourceDualRole",
                                PowerStatus::DUAL_ROLE_USB);
}

void PowerHandler::InitializePage() {
  if (this->show_power_status_)
    PowerStatus::Get()->RequestStatusUpdate();
}

void PowerHandler::RegisterMessages() {
  if (this->show_power_status_)
    PowerStatus::Get()->AddObserver(this);
  // Callback to fetch the power info.
  web_ui()->RegisterMessageCallback(
      "updatePowerStatus",
      base::Bind(&PowerHandler::UpdatePowerStatus, base::Unretained(this)));
  // Callback to set the power source.
  web_ui()->RegisterMessageCallback(
      "setPowerSource",
      base::Bind(&PowerHandler::SetPowerSource, base::Unretained(this)));
}

void PowerHandler::OnPowerStatusChanged() {
  web_ui()->CallJavascriptFunction(
      "options.PowerOverlay.setBatteryStatusText",
      base::StringValue(GetStatusValue()));
  UpdatePowerSources();
}

base::string16 PowerHandler::GetStatusValue() const {
  PowerStatus* status = PowerStatus::Get();
  if (!status->IsBatteryPresent())
    return base::string16();

  bool charging = status->IsBatteryCharging();
  bool calculating = status->IsBatteryTimeBeingCalculated();
  int percent = status->GetRoundedBatteryPercent();
  base::TimeDelta time_left;
  bool show_time = false;

  if (!calculating) {
    time_left = charging ? status->GetBatteryTimeToFull() :
                           status->GetBatteryTimeToEmpty();
    show_time = PowerStatus::ShouldDisplayBatteryTime(time_left);
  }

  if (!show_time) {
    return l10n_util::GetStringFUTF16(IDS_OPTIONS_BATTERY_STATUS_SHORT,
                                      base::IntToString16(percent));
  } else {
    int hour = 0;
    int min = 0;
    PowerStatus::SplitTimeIntoHoursAndMinutes(time_left, &hour, &min);
    base::string16 minute_text = base::ASCIIToUTF16(min < 10 ? "0" : "") +
                                 base::IntToString16(min);
    return l10n_util::GetStringFUTF16(
        charging ? IDS_OPTIONS_BATTERY_STATUS_CHARGING :
                   IDS_OPTIONS_BATTERY_STATUS,
        base::IntToString16(percent),
        base::IntToString16(hour),
        minute_text);
  }
}

void PowerHandler::UpdatePowerStatus(const base::ListValue* args) {
  PowerStatus::Get()->RequestStatusUpdate();
}

void PowerHandler::SetPowerSource(const base::ListValue* args) {
  std::string id;
  if (!args->GetString(0, &id)) {
    NOTREACHED();
    return;
  }
  PowerStatus::Get()->SetPowerSource(id);
}

void PowerHandler::UpdatePowerSources() {
  PowerStatus* status = PowerStatus::Get();

  base::ListValue sources_list;
  for (const auto& source : status->GetPowerSources()) {
    scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("id", source.id);
    dict->SetInteger("type", source.type);
    int message_id = GetPowerSourceDescription(source);
    dict->SetString("description", l10n_util::GetStringUTF16(message_id));
    sources_list.Append(dict.release());
  }

  web_ui()->CallJavascriptFunction(
      "options.PowerOverlay.setPowerSources",
      sources_list,
      base::StringValue(status->GetCurrentPowerSourceID()),
      base::FundamentalValue(status->IsUsbChargerConnected()),
      base::FundamentalValue(status->IsBatteryTimeBeingCalculated()));
}

}  // namespace options
}  // namespace chromeos
