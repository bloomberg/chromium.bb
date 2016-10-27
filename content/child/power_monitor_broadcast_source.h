// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_POWER_MONITOR_BROADCAST_SOURCE_H_
#define CONTENT_CHILD_POWER_MONITOR_BROADCAST_SOURCE_H_

#include "base/macros.h"
#include "base/power_monitor/power_monitor_source.h"
#include "content/common/content_export.h"
#include "device/power_monitor/public/interfaces/power_monitor.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {

// Receives Power Monitor IPC messages sent from the browser process and relays
// them to the PowerMonitor of the current process.
class CONTENT_EXPORT PowerMonitorBroadcastSource
    : public base::PowerMonitorSource,
      NON_EXPORTED_BASE(public device::mojom::PowerMonitorClient) {
 public:
  explicit PowerMonitorBroadcastSource();
  ~PowerMonitorBroadcastSource() override;

  void PowerStateChange(bool on_battery_power) override;
  void Suspend() override;
  void Resume() override;

 private:
  bool IsOnBatteryPowerImpl() override;
  bool last_reported_battery_power_state_;
  mojo::Binding<device::mojom::PowerMonitorClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorBroadcastSource);
};

}  // namespace content

#endif  // CONTENT_CHILD_POWER_MONITOR_BROADCAST_SOURCE_H_
