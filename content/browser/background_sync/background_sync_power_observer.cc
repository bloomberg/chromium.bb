// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_power_observer.h"

#include "base/power_monitor/power_monitor.h"
#include "base/thread_task_runner_handle.h"

namespace content {

BackgroundSyncPowerObserver::BackgroundSyncPowerObserver(
    const base::Closure& power_callback)
    : observing_power_monitor_(false),
      on_battery_(true),
      power_changed_callback_(power_callback) {
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor) {
    observing_power_monitor_ = true;
    on_battery_ = power_monitor->IsOnBatteryPower();
    power_monitor->AddObserver(this);
  }
}

BackgroundSyncPowerObserver::~BackgroundSyncPowerObserver() {
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();

  if (power_monitor)
    power_monitor->RemoveObserver(this);
}

bool BackgroundSyncPowerObserver::PowerSufficient(
    SyncPowerState power_state) const {
  DCHECK(observing_power_monitor_);
  DCHECK(base::PowerMonitor::Get());

  switch (power_state) {
    case POWER_STATE_AUTO:
      // TODO(jkarlin): Also check for device status, such as power saving mode
      // or user preferences. crbug.com/482088.
      return true;
    case POWER_STATE_AVOID_DRAINING:
      return !on_battery_;
  }

  NOTREACHED();
  return false;
}

void BackgroundSyncPowerObserver::OnPowerStateChange(bool on_battery_power) {
  DCHECK(observing_power_monitor_);
  DCHECK(base::PowerMonitor::Get());

  if (on_battery_ == on_battery_power)
    return;

  on_battery_ = on_battery_power;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                power_changed_callback_);
}

}  // namespace content
