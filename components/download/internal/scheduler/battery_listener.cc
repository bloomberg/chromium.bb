// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/scheduler/battery_listener.h"

#include "base/power_monitor/power_monitor.h"

namespace download {

// Helper function that converts the battery status to battery requirement.
BatteryStatus ToBatteryStatus(bool on_battery_power) {
  return on_battery_power ? BatteryStatus::NOT_CHARGING
                          : BatteryStatus::CHARGING;
}

BatteryListener::BatteryListener() = default;

BatteryListener::~BatteryListener() {
  Stop();
}

BatteryStatus BatteryListener::CurrentBatteryStatus() const {
  return ToBatteryStatus(base::PowerMonitor::Get()->IsOnBatteryPower());
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
  NotifyBatteryChange(ToBatteryStatus(on_battery_power));
}

void BatteryListener::NotifyBatteryChange(BatteryStatus battery_status) {
  for (auto& observer : observers_)
    observer.OnBatteryChange(battery_status);
}

}  // namespace download
