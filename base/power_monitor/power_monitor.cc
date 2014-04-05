// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor.h"

#include "base/lazy_instance.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/synchronization/lock.h"

namespace base {

class PowerMonitorImpl;

LazyInstance<Lock>::Leaky g_power_monitor_lock = LAZY_INSTANCE_INITIALIZER;
static PowerMonitorImpl* g_power_monitor = NULL;

// A class used to monitor the power state change and notify the observers about
// the change event.
class BASE_EXPORT PowerMonitorImpl {
 public:
  explicit PowerMonitorImpl(scoped_ptr<PowerMonitorSource> source)
      : observers_(new ObserverListThreadSafe<PowerObserver>()),
      source_(source.Pass()) { }

  ~PowerMonitorImpl()  { }

  scoped_refptr<ObserverListThreadSafe<PowerObserver> > observers_;
  scoped_ptr<PowerMonitorSource> source_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorImpl);
};

void PowerMonitor::Initialize(scoped_ptr<PowerMonitorSource> source) {
  AutoLock(g_power_monitor_lock.Get());
  DCHECK(!g_power_monitor);
  g_power_monitor = new PowerMonitorImpl(source.Pass());
}

void PowerMonitor::ShutdownForTesting() {
  AutoLock(g_power_monitor_lock.Get());
  if (g_power_monitor) {
    delete g_power_monitor;
    g_power_monitor = NULL;
  }
}

bool PowerMonitor::IsInitialized() {
  AutoLock(g_power_monitor_lock.Get());
  return g_power_monitor != NULL;
}

bool PowerMonitor::AddObserver(PowerObserver* observer) {
  AutoLock(g_power_monitor_lock.Get());
  if (!g_power_monitor)
    return false;
  g_power_monitor->observers_->AddObserver(observer);
  return true;
}

bool PowerMonitor::RemoveObserver(PowerObserver* observer) {
  AutoLock(g_power_monitor_lock.Get());
  if (!g_power_monitor)
    return false;
  g_power_monitor->observers_->RemoveObserver(observer);
  return true;
}

bool PowerMonitor::IsOnBatteryPower() {
  g_power_monitor_lock.Get().AssertAcquired();
  if (!g_power_monitor)
    return false;
  return g_power_monitor->source_->IsOnBatteryPower();
}

Lock* PowerMonitor::GetLock() {
  return &g_power_monitor_lock.Get();
}

bool PowerMonitor::IsInitializedLocked() {
  g_power_monitor_lock.Get().AssertAcquired();
  return g_power_monitor != NULL;
}

PowerMonitorSource* PowerMonitor::GetSource() {
  g_power_monitor_lock.Get().AssertAcquired();
  return g_power_monitor->source_.get();
}

void PowerMonitor::NotifyPowerStateChange(bool battery_in_use) {
  DVLOG(1) << "PowerStateChange: " << (battery_in_use ? "On" : "Off")
           << " battery";
  g_power_monitor_lock.Get().AssertAcquired();
  g_power_monitor->observers_->Notify(&PowerObserver::OnPowerStateChange,
      battery_in_use);
}

void PowerMonitor::NotifySuspend() {
  DVLOG(1) << "Power Suspending";
  g_power_monitor_lock.Get().AssertAcquired();
  g_power_monitor->observers_->Notify(&PowerObserver::OnSuspend);
}

void PowerMonitor::NotifyResume() {
  DVLOG(1) << "Power Resuming";
  g_power_monitor_lock.Get().AssertAcquired();
  g_power_monitor->observers_->Notify(&PowerObserver::OnResume);
}

}  // namespace base
