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
}

void PowerHandler::InitializePage() {
  if (this->show_power_status_)
    PowerStatus::Get()->RequestStatusUpdate();
}

void PowerHandler::RegisterMessages() {
  if (this->show_power_status_)
    PowerStatus::Get()->AddObserver(this);
}

void PowerHandler::OnPowerStatusChanged() {
  web_ui()->CallJavascriptFunction(
      "options.PowerOverlay.setBatteryStatusText",
      base::StringValue(GetStatusValue()));
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

}  // namespace options
}  // namespace chromeos
