// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_ble_connection_manager.h"

namespace chromeos {

namespace tether {

FakeBleConnectionManager::StatusAndRegisteredMessageTypes::
    StatusAndRegisteredMessageTypes()
    : status(cryptauth::SecureChannel::Status::DISCONNECTED) {}

FakeBleConnectionManager::StatusAndRegisteredMessageTypes::
    StatusAndRegisteredMessageTypes(
        const StatusAndRegisteredMessageTypes& other)
    : status(other.status),
      registered_message_types(other.registered_message_types) {}

FakeBleConnectionManager::StatusAndRegisteredMessageTypes::
    ~StatusAndRegisteredMessageTypes() {}

FakeBleConnectionManager::FakeBleConnectionManager()
    : BleConnectionManager(nullptr, nullptr, nullptr, nullptr, nullptr) {}

FakeBleConnectionManager::~FakeBleConnectionManager() {}

void FakeBleConnectionManager::SetDeviceStatus(
    const cryptauth::RemoteDevice& remote_device,
    const cryptauth::SecureChannel::Status& status) {
  const auto iter = device_map_.find(remote_device);
  DCHECK(iter != device_map_.end());

  cryptauth::SecureChannel::Status old_status = iter->second.status;
  if (old_status == status) {
    // If the status has not changed, do not do anything.
    return;
  }

  iter->second.status = status;
  SendSecureChannelStatusChangeEvent(remote_device, old_status, status);
}

void FakeBleConnectionManager::ReceiveMessage(
    const cryptauth::RemoteDevice& remote_device,
    const std::string& payload) {
  DCHECK(device_map_.find(remote_device) != device_map_.end());
  SendMessageReceivedEvent(remote_device, payload);
}

void FakeBleConnectionManager::RegisterRemoteDevice(
    const cryptauth::RemoteDevice& remote_device,
    const MessageType& connection_reason) {
  StatusAndRegisteredMessageTypes& value = device_map_[remote_device];
  value.registered_message_types.insert(connection_reason);
}

void FakeBleConnectionManager::UnregisterRemoteDevice(
    const cryptauth::RemoteDevice& remote_device,
    const MessageType& connection_reason) {
  StatusAndRegisteredMessageTypes& value = device_map_[remote_device];
  value.registered_message_types.erase(connection_reason);
  if (value.registered_message_types.empty()) {
    device_map_.erase(remote_device);
  }
}

void FakeBleConnectionManager::SendMessage(
    const cryptauth::RemoteDevice& remote_device,
    const std::string& message) {
  sent_messages_.push_back({remote_device, message});
}

bool FakeBleConnectionManager::GetStatusForDevice(
    const cryptauth::RemoteDevice& remote_device,
    cryptauth::SecureChannel::Status* status) const {
  const auto iter = device_map_.find(remote_device);
  if (iter == device_map_.end()) {
    return false;
  }

  *status = iter->second.status;
  return true;
}

}  // namespace tether

}  // namespace chromeos
