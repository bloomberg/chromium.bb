// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system_monitor/system_monitor.h"

#include <utility>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time.h"

namespace base {

static SystemMonitor* g_system_monitor = NULL;

#if defined(ENABLE_BATTERY_MONITORING)
// The amount of time (in ms) to wait before running the initial
// battery check.
static int kDelayedBatteryCheckMs = 10 * 1000;
#endif  // defined(ENABLE_BATTERY_MONITORING)

SystemMonitor::SystemMonitor()
    : power_observer_list_(new ObserverListThreadSafe<PowerObserver>()),
      devices_changed_observer_list_(
          new ObserverListThreadSafe<DevicesChangedObserver>()),
      battery_in_use_(false),
      suspended_(false) {
  DCHECK(!g_system_monitor);
  g_system_monitor = this;

  DCHECK(MessageLoop::current());
#if defined(ENABLE_BATTERY_MONITORING)
  delayed_battery_check_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kDelayedBatteryCheckMs), this,
      &SystemMonitor::BatteryCheck);
#endif  // defined(ENABLE_BATTERY_MONITORING)
#if defined(OS_MACOSX)
  PlatformInit();
#endif
}

SystemMonitor::~SystemMonitor() {
#if defined(OS_MACOSX)
  PlatformDestroy();
#endif
  DCHECK_EQ(this, g_system_monitor);
  g_system_monitor = NULL;
}

// static
SystemMonitor* SystemMonitor::Get() {
  return g_system_monitor;
}

void SystemMonitor::ProcessPowerMessage(PowerEvent event_id) {
  // Suppress duplicate notifications.  Some platforms may
  // send multiple notifications of the same event.
  switch (event_id) {
    case POWER_STATE_EVENT:
      {
        bool on_battery = IsBatteryPower();
        if (on_battery != battery_in_use_) {
          battery_in_use_ = on_battery;
          NotifyPowerStateChange();
        }
      }
      break;
    case RESUME_EVENT:
      if (suspended_) {
        suspended_ = false;
        NotifyResume();
      }
      break;
    case SUSPEND_EVENT:
      if (!suspended_) {
        suspended_ = true;
        NotifySuspend();
      }
      break;
  }
}

void SystemMonitor::ProcessDevicesChanged(DeviceType device_type) {
  NotifyDevicesChanged(device_type);
}

void SystemMonitor::AddPowerObserver(PowerObserver* obs) {
  power_observer_list_->AddObserver(obs);
}

void SystemMonitor::RemovePowerObserver(PowerObserver* obs) {
  power_observer_list_->RemoveObserver(obs);
}

void SystemMonitor::AddDevicesChangedObserver(DevicesChangedObserver* obs) {
  devices_changed_observer_list_->AddObserver(obs);
}

void SystemMonitor::RemoveDevicesChangedObserver(DevicesChangedObserver* obs) {
  devices_changed_observer_list_->RemoveObserver(obs);
}

void SystemMonitor::NotifyDevicesChanged(DeviceType device_type) {
  DVLOG(1) << "DevicesChanged with device type " << device_type;
  devices_changed_observer_list_->Notify(
      &DevicesChangedObserver::OnDevicesChanged, device_type);
}

void SystemMonitor::NotifyPowerStateChange() {
  DVLOG(1) << "PowerStateChange: " << (BatteryPower() ? "On" : "Off")
           << " battery";
  power_observer_list_->Notify(&PowerObserver::OnPowerStateChange,
                               BatteryPower());
}

void SystemMonitor::NotifySuspend() {
  DVLOG(1) << "Power Suspending";
  power_observer_list_->Notify(&PowerObserver::OnSuspend);
}

void SystemMonitor::NotifyResume() {
  DVLOG(1) << "Power Resuming";
  power_observer_list_->Notify(&PowerObserver::OnResume);
}

void SystemMonitor::BatteryCheck() {
  ProcessPowerMessage(SystemMonitor::POWER_STATE_EVENT);
}

}  // namespace base
