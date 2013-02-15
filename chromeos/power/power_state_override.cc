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
      dbus_thread_manager_(DBusThreadManager::Get()) {
  switch (mode) {
    case BLOCK_DISPLAY_SLEEP:
      override_types_ |= (PowerManagerClient::DISABLE_IDLE_DIM |
                          PowerManagerClient::DISABLE_IDLE_BLANK);
      // fallthrough
    case BLOCK_SYSTEM_SUSPEND:
      override_types_ |= PowerManagerClient::DISABLE_IDLE_SUSPEND;
      break;
    default:
      NOTREACHED() << "Unhandled mode " << mode;
  }

  dbus_thread_manager_->AddObserver(this);

  // request_id_ = 0 will create a new override request.
  // We do a post task here to ensure that this request runs 'after' our
  // constructor is done. If not, there is a possibility (though only in
  // tests at the moment) that the power state override request executes
  // and returns before the constructor has finished executing. This will
  // cause an AddRef and a Release, the latter destructing our current
  // instance even before the constructor has finished executing (as it does
  // in the DownloadExtensionTest browsertests currently).
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PowerStateOverride::CallRequestPowerStateOverrides, this));

  heartbeat_.Start(FROM_HERE,
                   base::TimeDelta::FromSeconds(kHeartbeatTimeInSecs),
                   this,
                   &PowerStateOverride::CallRequestPowerStateOverrides);
}

PowerStateOverride::~PowerStateOverride() {
  if (dbus_thread_manager_)
    dbus_thread_manager_->RemoveObserver(this);
  CancelRequest();
}

void PowerStateOverride::OnDBusThreadManagerDestroying(
    DBusThreadManager* manager) {
  DCHECK_EQ(manager, dbus_thread_manager_);
  CancelRequest();
  dbus_thread_manager_ = NULL;
}

void PowerStateOverride::SetRequestId(uint32 request_id) {
  request_id_ = request_id;
}

void PowerStateOverride::CallRequestPowerStateOverrides() {
  DCHECK(dbus_thread_manager_);
  dbus_thread_manager_->GetPowerManagerClient()->RequestPowerStateOverrides(
      request_id_,
      base::TimeDelta::FromSeconds(
          kHeartbeatTimeInSecs + kRequestSlackInSecs),
      override_types_,
      base::Bind(&PowerStateOverride::SetRequestId, this));
}

void PowerStateOverride::CancelRequest() {
  if (request_id_) {
    DCHECK(dbus_thread_manager_);
    dbus_thread_manager_->GetPowerManagerClient()->
        CancelPowerStateOverrides(request_id_);
    request_id_ = 0;
  }
}

}  // namespace chromeos
