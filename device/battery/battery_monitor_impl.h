// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "device/battery/battery_export.h"
#include "device/battery/battery_monitor.mojom.h"
#include "device/battery/battery_status_service.h"

#ifndef DEVICE_BATTERY_BATTERY_MONITOR_IMPL_H_
#define DEVICE_BATTERY_BATTERY_MONITOR_IMPL_H_

namespace device {

class BatteryMonitorImpl : public mojo::InterfaceImpl<BatteryMonitor> {
 public:
  DEVICE_BATTERY_EXPORT static void Create(
      mojo::InterfaceRequest<BatteryMonitor> request);

 private:
  BatteryMonitorImpl();
  ~BatteryMonitorImpl() override;

  // mojo::InterfaceImpl<..> methods:
  void OnConnectionEstablished() override;

  void DidChange(const BatteryStatus& battery_status);

  scoped_ptr<BatteryStatusService::BatteryUpdateSubscription> subscription_;
};

}  // namespace device

#endif  // DEVICE_BATTERY_BATTERY_MONITOR_IMPL_H_
