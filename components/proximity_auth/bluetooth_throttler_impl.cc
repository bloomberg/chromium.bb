// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/bluetooth_throttler_impl.h"

#include <utility>

#include "base/stl_util.h"
#include "base/time/tick_clock.h"
#include "components/proximity_auth/connection.h"

namespace proximity_auth {
namespace {

// Time to wait after disconnect before reconnecting.
const int kCooldownTimeSecs = 7;

}  // namespace

BluetoothThrottlerImpl::BluetoothThrottlerImpl(
    std::unique_ptr<base::TickClock> clock)
    : clock_(std::move(clock)) {}

BluetoothThrottlerImpl::~BluetoothThrottlerImpl() {
  for (Connection* connection : connections_) {
    connection->RemoveObserver(this);
  }
}

base::TimeDelta BluetoothThrottlerImpl::GetDelay() const {
  if (last_disconnect_time_.is_null())
    return base::TimeDelta();

  base::TimeTicks now = clock_->NowTicks();
  base::TimeTicks throttled_start_time =
      last_disconnect_time_ + GetCooldownTimeDelta();
  if (now >= throttled_start_time)
    return base::TimeDelta();

  return throttled_start_time - now;
}

void BluetoothThrottlerImpl::OnConnection(Connection* connection) {
  DCHECK(!ContainsKey(connections_, connection));
  connections_.insert(connection);
  connection->AddObserver(this);
}

base::TimeDelta BluetoothThrottlerImpl::GetCooldownTimeDelta() const {
  return base::TimeDelta::FromSeconds(kCooldownTimeSecs);
}

void BluetoothThrottlerImpl::OnConnectionStatusChanged(
    Connection* connection,
    Connection::Status old_status,
    Connection::Status new_status) {
  DCHECK(ContainsKey(connections_, connection));
  if (new_status == Connection::DISCONNECTED) {
    last_disconnect_time_ = clock_->NowTicks();
    connection->RemoveObserver(this);
    connections_.erase(connection);
  }
}

}  // namespace proximity_auth
