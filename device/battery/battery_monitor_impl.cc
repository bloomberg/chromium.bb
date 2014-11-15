// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/battery/battery_monitor_impl.h"

#include "base/bind.h"

namespace device {

// static
void BatteryMonitorImpl::Create(
    mojo::InterfaceRequest<BatteryMonitor> request) {
  BindToRequest(new BatteryMonitorImpl(), &request);
}

BatteryMonitorImpl::BatteryMonitorImpl() {
}

BatteryMonitorImpl::~BatteryMonitorImpl() {
}

void BatteryMonitorImpl::OnConnectionEstablished() {
  subscription_ = BatteryStatusService::GetInstance()->AddCallback(
        base::Bind(&BatteryMonitorImpl::DidChange, base::Unretained(this)));
}

void BatteryMonitorImpl::DidChange(const BatteryStatus& battery_status) {
  BatteryStatusPtr status(BatteryStatus::New());
  *status = battery_status;
  client()->DidChange(status.Pass());
}

}  // namespace device
