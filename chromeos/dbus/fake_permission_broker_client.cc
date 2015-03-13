// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_permission_broker_client.h"

#include "base/callback.h"
#include "base/logging.h"
#include "dbus/file_descriptor.h"

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

void FakePermissionBrokerClient::RequestTcpPortAccess(
    uint16 port,
    const std::string& interface,
    const dbus::FileDescriptor& lifeline_fd,
    const ResultCallback& callback) {
  DCHECK(lifeline_fd.is_valid());
  callback.Run(true);
}

void FakePermissionBrokerClient::RequestUdpPortAccess(
    uint16 port,
    const std::string& interface,
    const dbus::FileDescriptor& lifeline_fd,
    const ResultCallback& callback) {
  DCHECK(lifeline_fd.is_valid());
  callback.Run(true);
}

void FakePermissionBrokerClient::ReleaseTcpPort(
    uint16 port,
    const std::string& interface,
    const ResultCallback& callback) {
  callback.Run(true);
}

void FakePermissionBrokerClient::ReleaseUdpPort(
    uint16 port,
    const std::string& interface,
    const ResultCallback& callback) {
  callback.Run(true);
}

}  // namespace chromeos
