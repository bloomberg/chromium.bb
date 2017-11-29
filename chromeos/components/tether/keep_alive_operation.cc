// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/keep_alive_operation.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
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
          connection_manager),
      remote_device_(device_to_connect),
      clock_(base::MakeUnique<base::DefaultClock>()) {}

KeepAliveOperation::~KeepAliveOperation() = default;

void KeepAliveOperation::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void KeepAliveOperation::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void KeepAliveOperation::OnDeviceAuthenticated(
    const cryptauth::RemoteDevice& remote_device) {
  DCHECK(remote_devices().size() == 1u && remote_devices()[0] == remote_device);
  keep_alive_tickle_request_start_time_ = clock_->Now();
  SendMessageToDevice(remote_device,
                      base::MakeUnique<MessageWrapper>(KeepAliveTickle()));
}

void KeepAliveOperation::OnMessageReceived(
    std::unique_ptr<MessageWrapper> message_wrapper,
    const cryptauth::RemoteDevice& remote_device) {
  if (message_wrapper->GetMessageType() !=
      MessageType::KEEP_ALIVE_TICKLE_RESPONSE) {
    // If another type of message has been received, ignore it.
    return;
  }

  if (!(remote_device == remote_device_)) {
    // If the message came from another device, ignore it.
    return;
  }

  KeepAliveTickleResponse* response =
      static_cast<KeepAliveTickleResponse*>(message_wrapper->GetProto().get());
  device_status_ = base::MakeUnique<DeviceStatus>(response->device_status());

  DCHECK(!keep_alive_tickle_request_start_time_.is_null());
  UMA_HISTOGRAM_TIMES(
      "InstantTethering.Performance.KeepAliveTickleResponseDuration",
      clock_->Now() - keep_alive_tickle_request_start_time_);

  // Now that a response has been received, the device can be unregistered.
  UnregisterDevice(remote_device);
}

void KeepAliveOperation::OnOperationFinished() {
  for (auto& observer : observer_list_) {
    // Note: If the operation did not complete successfully, |device_status_|
    // will still be null.
    observer.OnOperationFinished(
        remote_device_, device_status_
                            ? base::MakeUnique<DeviceStatus>(*device_status_)
                            : nullptr);
  }
}

MessageType KeepAliveOperation::GetMessageTypeForConnection() {
  return MessageType::KEEP_ALIVE_TICKLE;
}

void KeepAliveOperation::SetClockForTest(
    std::unique_ptr<base::Clock> clock_for_test) {
  clock_ = std::move(clock_for_test);
}

}  // namespace tether

}  // namespace chromeos
