// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_peer_daemon_manager_client.h"

namespace chromeos {

FakePeerDaemonManagerClient::FakePeerDaemonManagerClient() {
}

FakePeerDaemonManagerClient::~FakePeerDaemonManagerClient() {
}

void FakePeerDaemonManagerClient::Init(dbus::Bus* bus) {
}

void FakePeerDaemonManagerClient::AddObserver(Observer* observer) {
}

void FakePeerDaemonManagerClient::RemoveObserver(Observer* observer) {
}

std::vector<dbus::ObjectPath> FakePeerDaemonManagerClient::GetPeers() {
  return std::vector<dbus::ObjectPath>();
}

std::vector<dbus::ObjectPath> FakePeerDaemonManagerClient::GetServices() {
  return std::vector<dbus::ObjectPath>();
}

PeerDaemonManagerClient::PeerProperties*
FakePeerDaemonManagerClient::GetPeerProperties(
    const dbus::ObjectPath& object_path) {
  return nullptr;
}

PeerDaemonManagerClient::ServiceProperties*
FakePeerDaemonManagerClient::GetServiceProperties(
    const dbus::ObjectPath& object_path) {
  return nullptr;
}

void FakePeerDaemonManagerClient::StartMonitoring(
    const std::vector<std::string>& requested_technologies,
    const base::DictionaryValue& options,
    const StringDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, "");
}

void FakePeerDaemonManagerClient::StopMonitoring(
    const std::string& monitoring_token,
    const VoidDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS);
}

void FakePeerDaemonManagerClient::ExposeService(
    const std::string& service_id,
    const std::map<std::string, std::string>& service_info,
    const base::DictionaryValue& options,
    const StringDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, "");
}

void FakePeerDaemonManagerClient::RemoveExposedService(
    const std::string& service_token,
    const VoidDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS);
}

void FakePeerDaemonManagerClient::Ping(
    const StringDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, "");
}

}  // namespace chromeos
