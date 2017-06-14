// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_status.h"

#include <algorithm>
#include <cmath>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"

namespace ash {
namespace {

// Updates |proto| to ensure that its fields are consistent.
void SanitizeProto(power_manager::PowerSupplyProperties* proto) {
  DCHECK(proto);

  if (proto->battery_state() ==
      power_manager::PowerSupplyProperties_BatteryState_FULL)
    proto->set_battery_percent(100.0);

  if (!proto->is_calculating_battery_time()) {
    const bool on_line_power =
        proto->external_power() !=
        power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED;
    if ((on_line_power && proto->battery_time_to_full_sec() < 0) ||
        (!on_line_power && proto->battery_time_to_empty_sec() < 0))
      proto->set_is_calculating_battery_time(true);
  }
}

base::string16 GetBatteryTimeAccessibilityString(int hour, int min) {
  DCHECK(hour || min);
  if (hour && !min) {
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG,
                                  base::TimeDelta::FromHours(hour));
  }
  if (min && !hour) {
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG,
                                  base::TimeDelta::FromMinutes(min));
  }
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BATTERY_TIME_ACCESSIBLE,
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG,
                             base::TimeDelta::FromHours(hour)),
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG,
                             base::TimeDelta::FromMinutes(min)));
}

int PowerSourceToMessageID(
    const power_manager::PowerSupplyProperties_PowerSource& source) {
  switch (source.port()) {
    case power_manager::PowerSupplyProperties_PowerSource_Port_UNKNOWN:
      return IDS_ASH_POWER_SOURCE_PORT_UNKNOWN;
    case power_manager::PowerSupplyProperties_PowerSource_Port_LEFT:
      return IDS_ASH_POWER_SOURCE_PORT_LEFT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_RIGHT:
      return IDS_ASH_POWER_SOURCE_PORT_RIGHT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_BACK:
      return IDS_ASH_POWER_SOURCE_PORT_BACK;
    case power_manager::PowerSupplyProperties_PowerSource_Port_FRONT:
      return IDS_ASH_POWER_SOURCE_PORT_FRONT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_LEFT_FRONT:
      return IDS_ASH_POWER_SOURCE_PORT_LEFT_FRONT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_LEFT_BACK:
      return IDS_ASH_POWER_SOURCE_PORT_LEFT_BACK;
    case power_manager::PowerSupplyProperties_PowerSource_Port_RIGHT_FRONT:
      return IDS_ASH_POWER_SOURCE_PORT_RIGHT_FRONT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_RIGHT_BACK:
      return IDS_ASH_POWER_SOURCE_PORT_RIGHT_BACK;
    case power_manager::PowerSupplyProperties_PowerSource_Port_BACK_LEFT:
      return IDS_ASH_POWER_SOURCE_PORT_BACK_LEFT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_BACK_RIGHT:
      return IDS_ASH_POWER_SOURCE_PORT_BACK_RIGHT;
  }
  NOTREACHED();
  return 0;
}

static PowerStatus* g_power_status = NULL;

// Minimum battery percentage rendered in UI.
const int kMinBatteryPercent = 1;

// The height of the battery icon (as measured from the user-visible bottom of
// the icon to the user-visible top of the icon).
const int kBatteryImageHeight = 12;

// The dimensions of the canvas containing the battery icon.
const int kBatteryCanvasSize = 16;

// The minimum height (in dp) of the charged region of the battery icon when the
// battery is present and has a charge greater than 0.
const int kMinVisualChargeLevel = 1;

// The empty background color of the battery icon in the system tray.
// TODO(tdanderson): Move these constants to a shared location if they are
// shared by more than one material design system icon.
const SkColor kBatteryBaseColor = SkColorSetA(SK_ColorWHITE, 0x4C);

// The background color of the charged region of the battery in the system
// tray.
const SkColor kBatteryChargeColor = SK_ColorWHITE;

// The color of the battery's badge (bolt, unreliable, X).
const SkColor kBatteryBadgeColor = SkColorSetA(SK_ColorBLACK, 0xB2);

// The color used for the battery's badge and charged color when the battery
// charge level is critically low.
const SkColor kBatteryAlertColor = SkColorSetRGB(0xDA, 0x27, 0x12);

}  // namespace

bool PowerStatus::BatteryImageInfo::operator==(
    const BatteryImageInfo& o) const {
  return icon_badge == o.icon_badge && charge_level == o.charge_level;
}

const int PowerStatus::kMaxBatteryTimeToDisplaySec = 24 * 60 * 60;

const double PowerStatus::kCriticalBatteryChargePercentage = 5;

// static
void PowerStatus::Initialize() {
  CHECK(!g_power_status);
  g_power_status = new PowerStatus();
}

// static
void PowerStatus::Shutdown() {
  CHECK(g_power_status);
  delete g_power_status;
  g_power_status = NULL;
}

// static
bool PowerStatus::IsInitialized() {
  return g_power_status != NULL;
}

// static
PowerStatus* PowerStatus::Get() {
  CHECK(g_power_status) << "PowerStatus::Get() called before Initialize().";
  return g_power_status;
}

// static
bool PowerStatus::ShouldDisplayBatteryTime(const base::TimeDelta& time) {
  return time >= base::TimeDelta::FromMinutes(1) &&
         time.InSeconds() <= kMaxBatteryTimeToDisplaySec;
}

// static
void PowerStatus::SplitTimeIntoHoursAndMinutes(const base::TimeDelta& time,
                                               int* hours,
                                               int* minutes) {
  DCHECK(hours);
  DCHECK(minutes);
  const int total_minutes = static_cast<int>(time.InSecondsF() / 60 + 0.5);
  *hours = total_minutes / 60;
  *minutes = total_minutes % 60;
}

void PowerStatus::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void PowerStatus::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void PowerStatus::RequestStatusUpdate() {
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->RequestStatusUpdate();
}

void PowerStatus::SetPowerSource(const std::string& id) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->SetPowerSource(
      id);
}

bool PowerStatus::IsBatteryPresent() const {
  return proto_.battery_state() !=
         power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT;
}

bool PowerStatus::IsBatteryFull() const {
  return proto_.battery_state() ==
         power_manager::PowerSupplyProperties_BatteryState_FULL;
}

bool PowerStatus::IsBatteryCharging() const {
  return proto_.battery_state() ==
         power_manager::PowerSupplyProperties_BatteryState_CHARGING;
}

bool PowerStatus::IsBatteryDischargingOnLinePower() const {
  return IsLinePowerConnected() &&
         proto_.battery_state() ==
             power_manager::PowerSupplyProperties_BatteryState_DISCHARGING;
}

double PowerStatus::GetBatteryPercent() const {
  return proto_.battery_percent();
}

int PowerStatus::GetRoundedBatteryPercent() const {
  return std::max(kMinBatteryPercent,
                  static_cast<int>(GetBatteryPercent() + 0.5));
}

bool PowerStatus::IsBatteryTimeBeingCalculated() const {
  return proto_.is_calculating_battery_time();
}

base::TimeDelta PowerStatus::GetBatteryTimeToEmpty() const {
  return base::TimeDelta::FromSeconds(proto_.battery_time_to_empty_sec());
}

base::TimeDelta PowerStatus::GetBatteryTimeToFull() const {
  return base::TimeDelta::FromSeconds(proto_.battery_time_to_full_sec());
}

bool PowerStatus::IsLinePowerConnected() const {
  return proto_.external_power() !=
         power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED;
}

bool PowerStatus::IsMainsChargerConnected() const {
  return proto_.external_power() ==
         power_manager::PowerSupplyProperties_ExternalPower_AC;
}

bool PowerStatus::IsUsbChargerConnected() const {
  return proto_.external_power() ==
         power_manager::PowerSupplyProperties_ExternalPower_USB;
}

bool PowerStatus::SupportsDualRoleDevices() const {
  return proto_.supports_dual_role_devices();
}

bool PowerStatus::HasDualRoleDevices() const {
  if (!SupportsDualRoleDevices())
    return false;

  for (int i = 0; i < proto_.available_external_power_source_size(); i++) {
    if (!proto_.available_external_power_source(i).active_by_default())
      return true;
  }
  return false;
}

std::vector<PowerStatus::PowerSource> PowerStatus::GetPowerSources() const {
  std::vector<PowerSource> sources;
  for (int i = 0; i < proto_.available_external_power_source_size(); i++) {
    const auto& source = proto_.available_external_power_source(i);
    sources.push_back(
        {source.id(),
         source.active_by_default() ? DEDICATED_CHARGER : DUAL_ROLE_USB,
         PowerSourceToMessageID(source)});
  }
  return sources;
}

std::string PowerStatus::GetCurrentPowerSourceID() const {
  return proto_.external_power_source_id();
}

PowerStatus::BatteryImageInfo PowerStatus::GetBatteryImageInfo() const {
  BatteryImageInfo info;
  CalculateBatteryImageInfo(&info);
  return info;
}

void PowerStatus::CalculateBatteryImageInfo(BatteryImageInfo* info) const {
  if (!IsUsbChargerConnected() && !IsBatteryPresent()) {
    info->icon_badge = &kSystemTrayBatteryXIcon;
    info->charge_level = 0;
    return;
  }

  if (IsUsbChargerConnected())
    info->icon_badge = &kSystemTrayBatteryUnreliableIcon;
  else if (IsLinePowerConnected())
    info->icon_badge = &kSystemTrayBatteryBoltIcon;
  else
    info->icon_badge = nullptr;

  // |charge_state| is a value between 0 and kBatteryImageHeight representing
  // the number of device pixels the battery image should be shown charged. The
  // exception is when |charge_state| is 0 (a critically-low battery); in this
  // case, still draw 1dp of charge.
  int charge_state =
      static_cast<int>(GetBatteryPercent() / 100.0 * kBatteryImageHeight);
  charge_state = std::max(std::min(charge_state, kBatteryImageHeight), 0);
  info->charge_level = std::max(charge_state, kMinVisualChargeLevel);

  // Use an alert badge if the battery is critically low and does not already
  // have a badge assigned.
  if (GetBatteryPercent() < kCriticalBatteryChargePercentage &&
      !info->icon_badge) {
    info->icon_badge = &kSystemTrayBatteryAlertIcon;
  }
}

// static
gfx::Size PowerStatus::GetBatteryImageSizeInDip() {
  return gfx::Size(kBatteryCanvasSize, kBatteryCanvasSize);
}

gfx::ImageSkiaRep PowerStatus::GetBatteryImage(const BatteryImageInfo& info,
                                               float scale) const {
  const bool use_alert_color =
      (info.charge_level == kMinVisualChargeLevel && !IsLinePowerConnected());
  const SkColor badge_color =
      use_alert_color ? kBatteryAlertColor : kBatteryBadgeColor;
  const SkColor charge_color =
      use_alert_color ? kBatteryAlertColor : kBatteryChargeColor;

  gfx::Canvas canvas(GetBatteryImageSizeInDip(), scale, false /* opaque */);

  // Routine to paint the icon image, aligned to pixel boundaries no matter what
  // the scale factor.
  auto paint_icon = [&canvas](SkColor color) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(color);

    SkPath path;
    gfx::ScopedCanvas scoped(&canvas);
    const float dsf = canvas.UndoDeviceScaleFactor();
    gfx::RectF bottom(4.5f, 3, 7, 11);
    bottom.Scale(dsf);
    path.addRoundRect(gfx::RectToSkRect(gfx::ToEnclosingRect(bottom)), 1, 1);

    gfx::RectF top(6.5f, 2, 3, 1);
    top.Scale(dsf);
    path.addRect(gfx::RectToSkRect(gfx::ToEnclosingRect(top)));
    canvas.DrawPath(path, flags);
  };

  // Paint the battery's base (background) color.
  paint_icon(kBatteryBaseColor);

  // Paint the charged portion of the battery. Note that |charge_height| adjusts
  // for the 2dp of padding between the bottom of the battery icon and the
  // bottom edge of |canvas|.
  const int charge_height = info.charge_level + 2;
  gfx::Rect clip_rect(0, kBatteryCanvasSize - charge_height, kBatteryCanvasSize,
                      charge_height);
  canvas.Save();
  canvas.ClipRect(clip_rect);
  paint_icon(charge_color);
  canvas.Restore();

  // Paint the badge over top of the battery, if applicable.
  if (info.icon_badge)
    PaintVectorIcon(&canvas, *info.icon_badge, kBatteryCanvasSize, badge_color);

  return gfx::ImageSkiaRep(canvas.GetBitmap(), scale);
}

base::string16 PowerStatus::GetAccessibleNameString(
    bool full_description) const {
  if (IsBatteryFull()) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_FULL_CHARGE_ACCESSIBLE);
  }

  base::string16 battery_percentage_accessible = l10n_util::GetStringFUTF16(
      IsBatteryCharging()
          ? IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_CHARGING_ACCESSIBLE
          : IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_ACCESSIBLE,
      base::IntToString16(GetRoundedBatteryPercent()));
  if (!full_description)
    return battery_percentage_accessible;

  base::string16 battery_time_accessible = base::string16();
  const base::TimeDelta time =
      IsBatteryCharging() ? GetBatteryTimeToFull() : GetBatteryTimeToEmpty();

  if (IsUsbChargerConnected()) {
    battery_time_accessible = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE_ACCESSIBLE);
  } else if (IsBatteryTimeBeingCalculated()) {
    battery_time_accessible = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING_ACCESSIBLE);
  } else if (ShouldDisplayBatteryTime(time) &&
             !IsBatteryDischargingOnLinePower()) {
    int hour = 0, min = 0;
    PowerStatus::SplitTimeIntoHoursAndMinutes(time, &hour, &min);
    base::string16 minute =
        min < 10 ? base::ASCIIToUTF16("0") + base::IntToString16(min)
                 : base::IntToString16(min);
    battery_time_accessible = l10n_util::GetStringFUTF16(
        IsBatteryCharging()
            ? IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL_ACCESSIBLE
            : IDS_ASH_STATUS_TRAY_BATTERY_TIME_LEFT_ACCESSIBLE,
        GetBatteryTimeAccessibilityString(hour, min));
  }
  return battery_time_accessible.empty()
             ? battery_percentage_accessible
             : battery_percentage_accessible + base::ASCIIToUTF16(". ") +
                   battery_time_accessible;
}

PowerStatus::PowerStatus() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->RequestStatusUpdate();
}

PowerStatus::~PowerStatus() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

void PowerStatus::SetProtoForTesting(
    const power_manager::PowerSupplyProperties& proto) {
  proto_ = proto;
  SanitizeProto(&proto_);
}

void PowerStatus::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  proto_ = proto;
  SanitizeProto(&proto_);
  for (auto& observer : observers_)
    observer.OnPowerStatusChanged();
}

}  // namespace ash
