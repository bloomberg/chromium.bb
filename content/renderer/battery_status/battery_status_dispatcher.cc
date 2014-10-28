// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/battery_status/battery_status_dispatcher.h"

#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/platform/WebBatteryStatusListener.h"

namespace content {

BatteryStatusDispatcher::BatteryStatusDispatcher(
    blink::WebBatteryStatusListener* listener)
    : listener_(listener) {
  DCHECK(listener_);

  RenderThread::Get()->GetServiceRegistry()->ConnectToRemoteService(&monitor_);
  monitor_.set_client(this);
}

BatteryStatusDispatcher::~BatteryStatusDispatcher() {
}

void BatteryStatusDispatcher::DidChange(
    device::BatteryStatusPtr battery_status) {
  DCHECK(battery_status);
  blink::WebBatteryStatus web_battery_status;
  web_battery_status.charging = battery_status->charging;
  web_battery_status.chargingTime = battery_status->charging_time;
  web_battery_status.dischargingTime = battery_status->discharging_time;
  web_battery_status.level = battery_status->level;
  listener_->updateBatteryStatus(web_battery_status);
}

}  // namespace content
