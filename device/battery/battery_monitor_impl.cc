// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/battery/battery_monitor_impl.h"

#include "base/bind.h"

namespace device {

// static
void BatteryMonitorImpl::Create(
    mojo::InterfaceRequest<BatteryMonitor> request) {
  new BatteryMonitorImpl(request.Pass());
}

BatteryMonitorImpl::BatteryMonitorImpl(
    mojo::InterfaceRequest<BatteryMonitor> request)
    : binding_(this, request.Pass()),
      subscription_(BatteryStatusService::GetInstance()->AddCallback(
          base::Bind(&BatteryMonitorImpl::DidChange, base::Unretained(this)))) {
}

BatteryMonitorImpl::~BatteryMonitorImpl() {
}

void BatteryMonitorImpl::RegisterSubscription() {
}

void BatteryMonitorImpl::DidChange(const BatteryStatus& battery_status) {
  BatteryStatusPtr status(BatteryStatus::New());
  *status = battery_status;
  binding_.client()->DidChange(status.Pass());
}

}  // namespace device
