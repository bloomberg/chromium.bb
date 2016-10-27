// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/power_monitor_broadcast_source.h"

#include "base/location.h"
#include "base/macros.h"
#include "content/child/child_thread_impl.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

PowerMonitorBroadcastSource::PowerMonitorBroadcastSource()
    : last_reported_battery_power_state_(false), binding_(this) {
  // May be null during test execution.
  if (ChildThreadImpl::current()) {
    device::mojom::PowerMonitorPtr power_monitor;
    ChildThreadImpl::current()->GetRemoteInterfaces()->GetInterface(
        mojo::GetProxy(&power_monitor));
    power_monitor->SetClient(binding_.CreateInterfacePtrAndBind());
  }
}

PowerMonitorBroadcastSource::~PowerMonitorBroadcastSource() {
}

bool PowerMonitorBroadcastSource::IsOnBatteryPowerImpl() {
  return last_reported_battery_power_state_;
}

void PowerMonitorBroadcastSource::PowerStateChange(bool on_battery_power) {
  last_reported_battery_power_state_ = on_battery_power;
  ProcessPowerEvent(PowerMonitorSource::POWER_STATE_EVENT);
}

void PowerMonitorBroadcastSource::Suspend() {
  ProcessPowerEvent(PowerMonitorSource::SUSPEND_EVENT);
}

void PowerMonitorBroadcastSource::Resume() {
  ProcessPowerEvent(PowerMonitorSource::RESUME_EVENT);
}

}  // namespace content
