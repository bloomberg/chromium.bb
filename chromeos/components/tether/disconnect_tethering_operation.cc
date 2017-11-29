// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/disconnect_tethering_operation.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

// static
DisconnectTetheringOperation::Factory*
    DisconnectTetheringOperation::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<DisconnectTetheringOperation>
DisconnectTetheringOperation::Factory::NewInstance(
    const cryptauth::RemoteDevice& device_to_connect,
    BleConnectionManager* connection_manager) {
  if (!factory_instance_) {
    factory_instance_ = new Factory();
  }
  return factory_instance_->BuildInstance(device_to_connect,
                                          connection_manager);
}

// static
void DisconnectTetheringOperation::Factory::SetInstanceForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

std::unique_ptr<DisconnectTetheringOperation>
DisconnectTetheringOperation::Factory::BuildInstance(
    const cryptauth::RemoteDevice& device_to_connect,
    BleConnectionManager* connection_manager) {
  return base::MakeUnique<DisconnectTetheringOperation>(device_to_connect,
                                                        connection_manager);
}

DisconnectTetheringOperation::DisconnectTetheringOperation(
    const cryptauth::RemoteDevice& device_to_connect,
    BleConnectionManager* connection_manager)
    : MessageTransferOperation(
          std::vector<cryptauth::RemoteDevice>{device_to_connect},
          connection_manager),
      remote_device_(device_to_connect),
      has_sent_message_(false),
      clock_(base::MakeUnique<base::DefaultClock>()) {}

DisconnectTetheringOperation::~DisconnectTetheringOperation() = default;

void DisconnectTetheringOperation::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void DisconnectTetheringOperation::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void DisconnectTetheringOperation::NotifyObserversOperationFinished(
    bool success) {
  for (auto& observer : observer_list_) {
    observer.OnOperationFinished(remote_device_.GetDeviceId(), success);
  }
}

void DisconnectTetheringOperation::OnDeviceAuthenticated(
    const cryptauth::RemoteDevice& remote_device) {
  DCHECK(remote_devices().size() == 1u && remote_devices()[0] == remote_device);

  disconnect_message_sequence_number_ = SendMessageToDevice(
      remote_device,
      base::MakeUnique<MessageWrapper>(DisconnectTetheringRequest()));
  disconnect_start_time_ = clock_->Now();
}

void DisconnectTetheringOperation::OnOperationFinished() {
  NotifyObserversOperationFinished(has_sent_message_);
}

MessageType DisconnectTetheringOperation::GetMessageTypeForConnection() {
  return MessageType::DISCONNECT_TETHERING_REQUEST;
}

void DisconnectTetheringOperation::OnMessageSent(int sequence_number) {
  if (sequence_number != disconnect_message_sequence_number_)
    return;

  has_sent_message_ = true;

  DCHECK(!disconnect_start_time_.is_null());
  UMA_HISTOGRAM_TIMES(
      "InstantTethering.Performance.DisconnectTetheringRequestDuration",
      clock_->Now() - disconnect_start_time_);
  disconnect_start_time_ = base::Time();

  UnregisterDevice(remote_device_);
}

void DisconnectTetheringOperation::SetClockForTest(
    std::unique_ptr<base::Clock> clock_for_test) {
  clock_ = std::move(clock_for_test);
}

}  // namespace tether

}  // namespace chromeos
