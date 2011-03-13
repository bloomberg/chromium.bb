// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/hi_res_timer_manager.h"

#include "base/time.h"

HighResolutionTimerManager::HighResolutionTimerManager()
    : hi_res_clock_used_(false) {
  ui::SystemMonitor* system_monitor = ui::SystemMonitor::Get();
  system_monitor->AddObserver(this);
  UseHiResClock(!system_monitor->BatteryPower());
}

HighResolutionTimerManager::~HighResolutionTimerManager() {
  ui::SystemMonitor::Get()->RemoveObserver(this);
  UseHiResClock(false);
}

void HighResolutionTimerManager::OnPowerStateChange(bool on_battery_power) {
  UseHiResClock(!on_battery_power);
}

void HighResolutionTimerManager::UseHiResClock(bool use) {
  if (use == hi_res_clock_used_)
    return;
  base::Time::EnableHighResolutionTimer(use);
}
