// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/message_transfer_operation.h"

#include <set>

#include "chromeos/components/tether/message_wrapper.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

namespace {

std::vector<cryptauth::RemoteDevice> RemoveDuplicatesFromVector(
    const std::vector<cryptauth::RemoteDevice>& remote_devices) {
  std::vector<cryptauth::RemoteDevice> updated_remote_devices;
  std::set<cryptauth::RemoteDevice> remote_devices_set;
  for (const auto& remote_device : remote_devices) {
    // Only add the device to the output vector if it has not already been put
    // into the set.
    if (remote_devices_set.find(remote_device) == remote_devices_set.end()) {
      remote_devices_set.insert(remote_device);
      updated_remote_devices.push_back(remote_device);
    }
  }
  return updated_remote_devices;
}

}  // namespace

// static
uint32_t MessageTransferOperation::kMaxConnectionAttemptsPerDevice = 3;

MessageTransferOperation::MessageTransferOperation(
    const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
    BleConnectionManager* connection_manager)
    : remote_devices_(RemoveDuplicatesFromVector(devices_to_connect)),
      connection_manager_(connection_manager),
      initialized_(false) {}

MessageTransferOperation::~MessageTransferOperation() {
  connection_manager_->RemoveObserver(this);
}

void MessageTransferOperation::Initialize() {
  if (initialized_) {
    return;
  }
  initialized_ = true;

  connection_manager_->AddObserver(this);
  OnOperationStarted();

  MessageType message_type_for_connection = GetMessageTypeForConnection();
  for (const auto& remote_device : remote_devices_) {
    connection_manager_->RegisterRemoteDevice(remote_device,
                                              message_type_for_connection);

    cryptauth::SecureChannel::Status status;
    if (connection_manager_->GetStatusForDevice(remote_device, &status) &&
        status == cryptauth::SecureChannel::Status::AUTHENTICATED) {
      OnDeviceAuthenticated(remote_device);
    }
  }
}

void MessageTransferOperation::OnSecureChannelStatusChanged(
    const cryptauth::RemoteDevice& remote_device,
    const cryptauth::SecureChannel::Status& old_status,
    const cryptauth::SecureChannel::Status& new_status) {
  if (std::find(remote_devices_.begin(), remote_devices_.end(),
                remote_device) == remote_devices_.end()) {
    // If the device whose status has changed does not correspond to any of the
    // devices passed to this MessageTransferOperation instance, ignore the
    // status change.
    return;
  }

  if (new_status == cryptauth::SecureChannel::Status::AUTHENTICATED) {
    OnDeviceAuthenticated(remote_device);
  } else if (old_status == cryptauth::SecureChannel::Status::AUTHENTICATING) {
    // If authentication fails, account details (e.g., BeaconSeeds) are not
    // synced, and there is no way to continue. Unregister the device and give
    // up.
    UnregisterDevice(remote_device);
  } else if (new_status == cryptauth::SecureChannel::Status::DISCONNECTED) {
    uint32_t num_attempts_so_far;
    if (remote_device_to_num_attempts_map_.find(remote_device) ==
        remote_device_to_num_attempts_map_.end()) {
      num_attempts_so_far = 0;
    } else {
      num_attempts_so_far = remote_device_to_num_attempts_map_[remote_device];
    }

    num_attempts_so_far++;
    remote_device_to_num_attempts_map_[remote_device] = num_attempts_so_far;

    PA_LOG(INFO) << "Connection attempt failed for device with ID "
                 << remote_device.GetTruncatedDeviceIdForLogs() << ". "
                 << "Number of failures so far: " << num_attempts_so_far;

    if (num_attempts_so_far >= kMaxConnectionAttemptsPerDevice) {
      PA_LOG(INFO) << "Connection retry limit reached for device with ID "
                   << remote_device.GetTruncatedDeviceIdForLogs() << ". "
                   << "Unregistering device.";

      // If the number of failures so far is equal to the maximum allowed number
      // of connection attempts, give up and unregister the device.
      UnregisterDevice(remote_device);
    }
  }
}

void MessageTransferOperation::OnMessageReceived(
    const cryptauth::RemoteDevice& remote_device,
    const std::string& payload) {
  if (std::find(remote_devices_.begin(), remote_devices_.end(),
                remote_device) == remote_devices_.end()) {
    // If the device from which the message has been received does not
    // correspond to any of the devices passed to this MessageTransferOperation
    // instance, ignore the message.
    return;
  }

  std::unique_ptr<MessageWrapper> message_wrapper =
      MessageWrapper::FromRawMessage(payload);
  if (message_wrapper) {
    OnMessageReceived(std::move(message_wrapper), remote_device);
  }
}

void MessageTransferOperation::UnregisterDevice(
    const cryptauth::RemoteDevice& remote_device) {
  remote_device_to_num_attempts_map_.erase(remote_device);
  remote_devices_.erase(std::remove(remote_devices_.begin(),
                                    remote_devices_.end(), remote_device),
                        remote_devices_.end());
  connection_manager_->UnregisterRemoteDevice(remote_device,
                                              GetMessageTypeForConnection());

  if (remote_devices_.empty()) {
    OnOperationFinished();
  }
}

void MessageTransferOperation::SendMessageToDevice(
    const cryptauth::RemoteDevice& remote_device,
    std::unique_ptr<MessageWrapper> message_wrapper) {
  connection_manager_->SendMessage(remote_device,
                                   message_wrapper->ToRawMessage());
}

}  // namespace tether

}  // namespace chromeos
