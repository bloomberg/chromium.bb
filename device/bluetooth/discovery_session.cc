// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "device/bluetooth/discovery_session.h"

namespace bluetooth {
DiscoverySession::DiscoverySession(
    std::unique_ptr<device::BluetoothDiscoverySession> session)
    : discovery_session_(std::move(session)), weak_ptr_factory_(this) {}

DiscoverySession::~DiscoverySession() {}

void DiscoverySession::IsActive(const IsActiveCallback& callback) {
  callback.Run(discovery_session_->IsActive());
}

void DiscoverySession::Stop(const StopCallback& callback) {
  discovery_session_->Stop(
      base::Bind(&DiscoverySession::OnStop, weak_ptr_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&DiscoverySession::OnStopError, weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void DiscoverySession::OnStop(const StopCallback& callback) {
  callback.Run(true /* success */);
}

void DiscoverySession::OnStopError(const StopCallback& callback) {
  callback.Run(false /* success */);
}

}  // namespace bluetooth
