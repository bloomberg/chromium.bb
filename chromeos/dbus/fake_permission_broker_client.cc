// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_permission_broker_client.h"

#include "base/callback.h"

namespace chromeos {

FakePermissionBrokerClient::FakePermissionBrokerClient() {}

FakePermissionBrokerClient::~FakePermissionBrokerClient() {}

void FakePermissionBrokerClient::Init(dbus::Bus* bus) {}

void FakePermissionBrokerClient::RequestPathAccess(
    const std::string& path,
    int interface_id,
    const ResultCallback& callback) {
  callback.Run(false);
}

void FakePermissionBrokerClient::RequestUsbAccess(
    const uint16_t vendor_id,
    const uint16_t product_id,
    int interface_id,
    const ResultCallback& callback) {
  callback.Run(false);
}

}  // namespace chromeos
