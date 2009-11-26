// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/system_monitor.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time.h"

static SystemMonitor* g_system_monitor = NULL;

#if defined(ENABLE_BATTERY_MONITORING)
// The amount of time (in ms) to wait before running the initial
// battery check.
static int kDelayedBatteryCheckMs = 10 * 1000;
#endif  // defined(ENABLE_BATTERY_MONITORING)

SystemMonitor::SystemMonitor()
    : observer_list_(new ObserverListThreadSafe<PowerObserver>()),
      battery_in_use_(false),
      suspended_(false) {
  DCHECK(!g_system_monitor);
  g_system_monitor = this;

  DCHECK(MessageLoop::current());
#if defined(ENABLE_BATTERY_MONITORING)
  delayed_battery_check_.Start(
      base::TimeDelta::FromMilliseconds(kDelayedBatteryCheckMs), this,
      &SystemMonitor::BatteryCheck);
#endif  // defined(ENABLE_BATTERY_MONITORING)
}

SystemMonitor::~SystemMonitor() {
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

void SystemMonitor::AddObserver(PowerObserver* obs) {
  observer_list_->AddObserver(obs);
}

void SystemMonitor::RemoveObserver(PowerObserver* obs) {
  observer_list_->RemoveObserver(obs);
}

void SystemMonitor::NotifyPowerStateChange() {
  LOG(INFO) << L"PowerStateChange: "
           << (BatteryPower() ? L"On" : L"Off") << L" battery";
  observer_list_->Notify(&PowerObserver::OnPowerStateChange, BatteryPower());
}

void SystemMonitor::NotifySuspend() {
  LOG(INFO) << L"Power Suspending";
  observer_list_->Notify(&PowerObserver::OnSuspend);
}

void SystemMonitor::NotifyResume() {
  LOG(INFO) << L"Power Resuming";
  observer_list_->Notify(&PowerObserver::OnResume);
}

void SystemMonitor::BatteryCheck() {
  ProcessPowerMessage(SystemMonitor::POWER_STATE_EVENT);
}
