// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/power/power_state_override.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

namespace {

// Frequency with which overrides are renewed.
const int kHeartbeatTimeInSecs = 300;

// Duration beyond |kHeartbeatTimeInSecs| for which overrides are requested.
// This should be long enough that we're able to renew the request before it
// expires, but short enough that the power manager won't end up honoring a
// stale request for a long time if Chrome crashes and orphans its requests.
const int kRequestSlackInSecs = 15;

}  // namespace

namespace chromeos {

PowerStateOverride::PowerStateOverride(Mode mode)
    : override_types_(0),
      request_id_(0),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  switch (mode) {
    case BLOCK_DISPLAY_SLEEP:
      override_types_ |= (PowerManagerClient::DISABLE_IDLE_DIM |
                          PowerManagerClient::DISABLE_IDLE_BLANK);
      // fallthrough
    case BLOCK_SYSTEM_SUSPEND:
      override_types_ |= (PowerManagerClient::DISABLE_IDLE_SUSPEND |
                          PowerManagerClient::DISABLE_IDLE_LID_SUSPEND);
      break;
    default:
      NOTREACHED() << "Unhandled mode " << mode;
  }

  // request_id_ = 0 will create a new override request.
  CallRequestPowerStateOverrides();

  heartbeat_.Start(FROM_HERE,
                   base::TimeDelta::FromSeconds(kHeartbeatTimeInSecs),
                   weak_ptr_factory_.GetWeakPtr(),
                   &PowerStateOverride::CallRequestPowerStateOverrides);
}

PowerStateOverride::~PowerStateOverride() {
  heartbeat_.Stop();

  PowerManagerClient* power_manager =
      DBusThreadManager::Get()->GetPowerManagerClient();
  if (power_manager)
    power_manager->CancelPowerStateOverrides(request_id_);
}

void PowerStateOverride::SetRequestId(uint32 request_id) {
  request_id_ = request_id;
}

void PowerStateOverride::CallRequestPowerStateOverrides() {
  PowerManagerClient* power_manager =
      DBusThreadManager::Get()->GetPowerManagerClient();
  if (power_manager) {
    power_manager->RequestPowerStateOverrides(
        request_id_,
        base::TimeDelta::FromSeconds(
            kHeartbeatTimeInSecs + kRequestSlackInSecs),
        override_types_,
        base::Bind(&PowerStateOverride::SetRequestId,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

}  // namespace chromeos
