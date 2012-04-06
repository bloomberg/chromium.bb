// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/power_state_override.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

using chromeos::PowerManagerClient;

namespace {

const int kHeartbeatTimeInSecs = 300;

}

namespace chromeos {

PowerStateOverride::PowerStateOverride()
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  // Call with request id = 0 to create a new override request.
  CallRequestPowerStateOverrides(0);
}

PowerStateOverride::~PowerStateOverride() {
}

void PowerStateOverride::Heartbeat(uint32 request_id) {
  // We've been called back from an override request, schedule another for after
  // kHeartbeatTime seconds.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PowerStateOverride::CallRequestPowerStateOverrides,
                 weak_ptr_factory_.GetWeakPtr(),
                 request_id),
      base::TimeDelta::FromSeconds(kHeartbeatTimeInSecs));
}

void PowerStateOverride::CallRequestPowerStateOverrides(
    uint32 request_id) {
  PowerManagerClient* power_manager =
      DBusThreadManager::Get()->GetPowerManagerClient();
  if (power_manager) {
    // Request the duration of twice our heartbeat.
    power_manager->RequestPowerStateOverrides(
        request_id,
        kHeartbeatTimeInSecs * 2,
        PowerManagerClient::DISABLE_IDLE_DIM |
        PowerManagerClient::DISABLE_IDLE_BLANK |
        PowerManagerClient::DISABLE_IDLE_SUSPEND |
        PowerManagerClient::DISABLE_IDLE_LID_SUSPEND,
        base::Bind(&PowerStateOverride::Heartbeat,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

}  // namespace chromeos
