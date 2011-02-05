// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/power_menu_button.h"

#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton

// static
const int PowerMenuButton::kNumPowerImages = 16;

PowerMenuButton::PowerMenuButton()
    : StatusAreaButton(this),
      battery_is_present_(false),
      line_power_on_(false),
      battery_fully_charged_(false),
      battery_percentage_(0.0),
      icon_id_(-1),
      ALLOW_THIS_IN_INITIALIZER_LIST(power_menu_(this)) {
  UpdateIconAndLabelInfo();
  CrosLibrary::Get()->GetPowerLibrary()->AddObserver(this);
}

PowerMenuButton::~PowerMenuButton() {
  CrosLibrary::Get()->GetPowerLibrary()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, ui::MenuModel implementation:

int PowerMenuButton::GetItemCount() const {
  return 2;
}

ui::MenuModel::ItemType PowerMenuButton::GetTypeAt(int index) const {
  return ui::MenuModel::TYPE_COMMAND;
}

string16 PowerMenuButton::GetLabelAt(int index) const {
  // The first item shows the percentage of battery left.
  if (index == 0) {
    return l10n_util::GetStringFUTF16(IDS_STATUSBAR_BATTERY_PERCENTAGE,
        base::IntToString16(static_cast<int>(battery_percentage_)));
  } else if (index == 1) {
    // The second item shows the battery is charged if it is.
    if (battery_fully_charged_)
      return l10n_util::GetStringUTF16(IDS_STATUSBAR_BATTERY_IS_CHARGED);

    // If battery is in an intermediate charge state, show how much time left.
    base::TimeDelta time = line_power_on_ ? battery_time_to_full_ :
        battery_time_to_empty_;
    if (time.InSeconds() == 0) {
      // If time is 0, then that means we are still calculating how much time.
      // Depending if line power is on, we either show a message saying that we
      // are calculating time until full or calculating remaining time.
      int msg = line_power_on_ ?
          IDS_STATUSBAR_BATTERY_CALCULATING_TIME_UNTIL_FULL :
          IDS_STATUSBAR_BATTERY_CALCULATING_TIME_UNTIL_EMPTY;
      return l10n_util::GetStringUTF16(msg);
    } else {
      // Depending if line power is on, we either show a message saying XX:YY
      // until full or XX:YY remaining where XX is number of hours and YY is
      // number of minutes.
      int msg = line_power_on_ ? IDS_STATUSBAR_BATTERY_TIME_UNTIL_FULL :
          IDS_STATUSBAR_BATTERY_TIME_UNTIL_EMPTY;
      int hour = time.InHours();
      int min = (time - base::TimeDelta::FromHours(hour)).InMinutes();
      string16 hour_str = base::IntToString16(hour);
      string16 min_str = base::IntToString16(min);
      // Append a "0" before the minute if it's only a single digit.
      if (min < 10)
        min_str = ASCIIToUTF16("0") + min_str;
      return l10n_util::GetStringFUTF16(msg, hour_str, min_str);
    }
  } else {
    NOTREACHED();
    return string16();
  }
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, views::ViewMenuDelegate implementation:

void PowerMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  power_menu_.Rebuild();
  power_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, PowerLibrary::Observer implementation:

void PowerMenuButton::PowerChanged(PowerLibrary* obj) {
  UpdateIconAndLabelInfo();
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, StatusAreaButton implementation:

void PowerMenuButton::UpdateIconAndLabelInfo() {
  PowerLibrary* cros = CrosLibrary::Get()->GetPowerLibrary();
  if (!cros)
    return;

  bool cros_loaded = CrosLibrary::Get()->EnsureLoaded();
  if (cros_loaded) {
    battery_is_present_ = cros->battery_is_present();
    line_power_on_ = cros->line_power_on();
    battery_fully_charged_ = cros->battery_fully_charged();
    battery_percentage_ = cros->battery_percentage();
    // If fully charged, always show 100% even if internal number is a bit less.
    // Note: we always call cros->battery_percentage() for test predictability.
    if (battery_fully_charged_)
      battery_percentage_ = 100.0;
    battery_time_to_full_ = cros->battery_time_to_full();
    battery_time_to_empty_ = cros->battery_time_to_empty();
  }

  if (!cros_loaded) {
    icon_id_ = IDR_STATUSBAR_BATTERY_UNKNOWN;
  } else if (!battery_is_present_) {
    icon_id_ = IDR_STATUSBAR_BATTERY_MISSING;
  } else if (line_power_on_ && battery_fully_charged_) {
    icon_id_ = IDR_STATUSBAR_BATTERY_CHARGED;
  } else {
    // Get the power image depending on battery percentage. Percentage is
    // from 0 to 100, so we need to convert that to 0 to kNumPowerImages - 1.
    // NOTE: Use an array rather than just calculating a resource number to
    // avoid creating implicit ordering dependencies on the resource values.
    static const int kChargingImages[kNumPowerImages] = {
      IDR_STATUSBAR_BATTERY_CHARGING_1,
      IDR_STATUSBAR_BATTERY_CHARGING_2,
      IDR_STATUSBAR_BATTERY_CHARGING_3,
      IDR_STATUSBAR_BATTERY_CHARGING_4,
      IDR_STATUSBAR_BATTERY_CHARGING_5,
      IDR_STATUSBAR_BATTERY_CHARGING_6,
      IDR_STATUSBAR_BATTERY_CHARGING_7,
      IDR_STATUSBAR_BATTERY_CHARGING_8,
      IDR_STATUSBAR_BATTERY_CHARGING_9,
      IDR_STATUSBAR_BATTERY_CHARGING_10,
      IDR_STATUSBAR_BATTERY_CHARGING_11,
      IDR_STATUSBAR_BATTERY_CHARGING_12,
      IDR_STATUSBAR_BATTERY_CHARGING_13,
      IDR_STATUSBAR_BATTERY_CHARGING_14,
      IDR_STATUSBAR_BATTERY_CHARGING_15,
      IDR_STATUSBAR_BATTERY_CHARGING_16,
    };
    static const int kDischargingImages[kNumPowerImages] = {
      IDR_STATUSBAR_BATTERY_DISCHARGING_1,
      IDR_STATUSBAR_BATTERY_DISCHARGING_2,
      IDR_STATUSBAR_BATTERY_DISCHARGING_3,
      IDR_STATUSBAR_BATTERY_DISCHARGING_4,
      IDR_STATUSBAR_BATTERY_DISCHARGING_5,
      IDR_STATUSBAR_BATTERY_DISCHARGING_6,
      IDR_STATUSBAR_BATTERY_DISCHARGING_7,
      IDR_STATUSBAR_BATTERY_DISCHARGING_8,
      IDR_STATUSBAR_BATTERY_DISCHARGING_9,
      IDR_STATUSBAR_BATTERY_DISCHARGING_10,
      IDR_STATUSBAR_BATTERY_DISCHARGING_11,
      IDR_STATUSBAR_BATTERY_DISCHARGING_12,
      IDR_STATUSBAR_BATTERY_DISCHARGING_13,
      IDR_STATUSBAR_BATTERY_DISCHARGING_14,
      IDR_STATUSBAR_BATTERY_DISCHARGING_15,
      IDR_STATUSBAR_BATTERY_DISCHARGING_16,
    };

    int index = static_cast<int>(battery_percentage_ / 100.0 *
                nextafter(static_cast<float>(kNumPowerImages), 0));
    index = std::max(std::min(index, kNumPowerImages - 1), 0);
    icon_id_ = line_power_on_ ?
        kChargingImages[index] : kDischargingImages[index];
  }

  SetIcon(*ResourceBundle::GetSharedInstance().GetBitmapNamed(icon_id_));
  SetTooltipText(UTF16ToWide(GetLabelAt(0)));
  power_menu_.Rebuild();
  SchedulePaint();
}

}  // namespace chromeos
