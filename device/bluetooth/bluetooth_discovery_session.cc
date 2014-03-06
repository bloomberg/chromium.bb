// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_discovery_session.h"

#include "base/bind.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

BluetoothDiscoverySession::BluetoothDiscoverySession(BluetoothAdapter* adapter)
    : active_(true),
      adapter_(adapter),
      weak_ptr_factory_(this) {
}

BluetoothDiscoverySession::~BluetoothDiscoverySession() {
  Stop(base::Bind(&base::DoNothing), base::Bind(&base::DoNothing));
  adapter_->DiscoverySessionDestroyed(this);
}

bool BluetoothDiscoverySession::IsActive() const {
  return active_;
}

void BluetoothDiscoverySession::Stop(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (!active_) {
    LOG(ERROR) << "Discovery session not active. Cannot stop.";
    error_callback.Run();
    return;
  }
  VLOG(1) << "Stopping device discovery session.";
  adapter_->RemoveDiscoverySession(
      base::Bind(&BluetoothDiscoverySession::OnStop,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      error_callback);
}

void BluetoothDiscoverySession::OnStop(const base::Closure& callback) {
  active_ = false;
  callback.Run();
}

void BluetoothDiscoverySession::MarkAsInactive() {
  active_ = false;
}

}  // namespace device
