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
#include "ui/gfx/image/image_skia.h"

using ash::PowerStatus;

namespace chromeos {
namespace options {

PowerHandler::PowerHandler() {
}

PowerHandler::~PowerHandler() {
  if (switches::PowerOverlayEnabled())
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
}

void PowerHandler::InitializePage() {
  if (switches::PowerOverlayEnabled())
    PowerStatus::Get()->RequestStatusUpdate();
}

void PowerHandler::RegisterMessages() {
  if (switches::PowerOverlayEnabled())
    PowerStatus::Get()->AddObserver(this);

  // Callback to fetch the battery icon data.
  web_ui()->RegisterMessageCallback(
      "requestBatteryIcon",
      base::Bind(&PowerHandler::GetBatteryIcon, base::Unretained(this)));
}

void PowerHandler::OnPowerStatusChanged() {
  web_ui()->CallJavascriptFunction(
      "options.PowerOverlay.setBatteryStatusText",
      base::StringValue(GetStatusValue()));
  web_ui()->CallJavascriptFunction(
      "options.BrowserOptions.setBatteryStatusText",
      base::StringValue(GetFullStatusText()));
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

base::string16 PowerHandler::GetFullStatusText() const {
  base::string16 status = GetStatusValue();
  if (status.empty())
    return base::string16();
  return l10n_util::GetStringFUTF16(
      IDS_OPTIONS_DEVICE_GROUP_BATTERY_STATUS_LABEL, status);
}

void PowerHandler::GetBatteryIcon(const base::ListValue* args) {
  gfx::ImageSkia icon = PowerStatus::Get()->GetBatteryImage(
      ash::PowerStatus::ICON_DARK);
  gfx::ImageSkiaRep image_rep = icon.GetRepresentation(
      web_ui()->GetDeviceScaleFactor());
  web_ui()->CallJavascriptFunction(
      "options.BrowserOptions.setBatteryIcon",
      base::StringValue(webui::GetBitmapDataUrl(image_rep.sk_bitmap())));
}

}  // namespace options
}  // namespace chromeos
