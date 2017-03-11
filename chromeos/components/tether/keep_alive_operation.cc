// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/keep_alive_operation.h"

#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

// static
KeepAliveOperation::Factory* KeepAliveOperation::Factory::factory_instance_ =
    nullptr;

// static
std::unique_ptr<KeepAliveOperation> KeepAliveOperation::Factory::NewInstance(
    const cryptauth::RemoteDevice& device_to_connect,
    BleConnectionManager* connection_manager) {
  if (!factory_instance_) {
    factory_instance_ = new Factory();
  }
  return factory_instance_->BuildInstance(device_to_connect,
                                          connection_manager);
}

// static
void KeepAliveOperation::Factory::SetInstanceForTesting(Factory* factory) {
  factory_instance_ = factory;
}

std::unique_ptr<KeepAliveOperation> KeepAliveOperation::Factory::BuildInstance(
    const cryptauth::RemoteDevice& device_to_connect,
    BleConnectionManager* connection_manager) {
  return base::MakeUnique<KeepAliveOperation>(device_to_connect,
                                              connection_manager);
}

KeepAliveOperation::KeepAliveOperation(
    const cryptauth::RemoteDevice& device_to_connect,
    BleConnectionManager* connection_manager)
    : MessageTransferOperation(
          std::vector<cryptauth::RemoteDevice>{device_to_connect},
          connection_manager) {}

KeepAliveOperation::~KeepAliveOperation() {}

void KeepAliveOperation::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void KeepAliveOperation::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void KeepAliveOperation::OnDeviceAuthenticated(
    const cryptauth::RemoteDevice& remote_device) {
  DCHECK(remote_devices().size() == 1u && remote_devices()[0] == remote_device);

  SendMessageToDevice(remote_device,
                      base::MakeUnique<MessageWrapper>(KeepAliveTickle()));
  UnregisterDevice(remote_device);
}

void KeepAliveOperation::OnOperationFinished() {
  for (auto& observer : observer_list_) {
    observer.OnOperationFinished();
  }
}

MessageType KeepAliveOperation::GetMessageTypeForConnection() {
  return MessageType::KEEP_ALIVE_TICKLE;
}

}  // namespace tether

}  // namespace chromeos
