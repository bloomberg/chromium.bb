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
    : request_id_(0),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  // request id_ = 0 will create a new override request.
  CallRequestPowerStateOverrides();

  // Start the heartbeat delayed, since we've just sent a request, we may not
  // have received our request_id back yet.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PowerStateOverride::StartHeartbeat,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kHeartbeatTimeInSecs));
}

PowerStateOverride::~PowerStateOverride() {
  heartbeat_.Stop();

  PowerManagerClient* power_manager =
      DBusThreadManager::Get()->GetPowerManagerClient();
  if (power_manager)
    power_manager->CancelPowerStateOverrides(request_id_);
}

void PowerStateOverride::StartHeartbeat() {
  heartbeat_.Start(FROM_HERE,
                   base::TimeDelta::FromSeconds(kHeartbeatTimeInSecs),
                   weak_ptr_factory_.GetWeakPtr(),
                   &PowerStateOverride::CallRequestPowerStateOverrides);
}

void PowerStateOverride::SetRequestId(uint32 request_id) {
  request_id_ = request_id;
}

void PowerStateOverride::CallRequestPowerStateOverrides() {
  PowerManagerClient* power_manager =
      DBusThreadManager::Get()->GetPowerManagerClient();
  if (power_manager) {
    // Request the duration of twice our heartbeat.
    power_manager->RequestPowerStateOverrides(
        request_id_,
        kHeartbeatTimeInSecs * 2,
        PowerManagerClient::DISABLE_IDLE_DIM |
        PowerManagerClient::DISABLE_IDLE_BLANK |
        PowerManagerClient::DISABLE_IDLE_SUSPEND |
        PowerManagerClient::DISABLE_IDLE_LID_SUSPEND,
        base::Bind(&PowerStateOverride::SetRequestId,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

}  // namespace chromeos
