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
      status_to_report_(false) {
  // NOTE: DidChange may be called before AddCallback returns. This is done to
  // report current status.
  subscription_ = BatteryStatusService::GetInstance()->AddCallback(
      base::Bind(&BatteryMonitorImpl::DidChange, base::Unretained(this)));
}

BatteryMonitorImpl::~BatteryMonitorImpl() {
}

void BatteryMonitorImpl::QueryNextStatus(
    const BatteryStatusCallback& callback) {
  callbacks_.push_back(callback);

  if (status_to_report_)
    ReportStatus();
}

void BatteryMonitorImpl::RegisterSubscription() {
}

void BatteryMonitorImpl::DidChange(const BatteryStatus& battery_status) {
  status_ = battery_status;
  status_to_report_ = true;

  if (!callbacks_.empty())
    ReportStatus();
}

void BatteryMonitorImpl::ReportStatus() {
  for (const auto& callback : callbacks_)
    callback.Run(status_.Clone());
  callbacks_.clear();

  status_to_report_ = false;
}

}  // namespace device
