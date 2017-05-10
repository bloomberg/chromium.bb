// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/scheduler/battery_listener.h"

#include "base/power_monitor/power_monitor.h"

namespace download {

// Helper function that converts the battery status to battery requirement.
SchedulingParams::BatteryRequirements ToBatteryRequirement(
    bool on_battery_power) {
  return on_battery_power
             ? SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE
             : SchedulingParams::BatteryRequirements::BATTERY_SENSITIVE;
}

BatteryListener::BatteryListener() = default;

BatteryListener::~BatteryListener() {
  Stop();
}

SchedulingParams::BatteryRequirements BatteryListener::CurrentBatteryStatus()
    const {
  return ToBatteryRequirement(base::PowerMonitor::Get()->IsOnBatteryPower());
}

void BatteryListener::Start() {
  base::PowerMonitor* monitor = base::PowerMonitor::Get();
  DCHECK(monitor);
  monitor->AddObserver(this);
}

void BatteryListener::Stop() {
  base::PowerMonitor::Get()->RemoveObserver(this);
}

void BatteryListener::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BatteryListener::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void BatteryListener::OnPowerStateChange(bool on_battery_power) {
  NotifyBatteryChange(ToBatteryRequirement(on_battery_power));
}

void BatteryListener::NotifyBatteryChange(
    SchedulingParams::BatteryRequirements current_battery) {
  for (auto& observer : observers_)
    observer.OnBatteryChange(current_battery);
}

}  // namespace download
