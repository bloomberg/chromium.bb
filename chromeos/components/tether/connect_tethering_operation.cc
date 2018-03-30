// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/connect_tethering_operation.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "chromeos/components/tether/tether_host_response_recorder.h"

namespace chromeos {

namespace tether {

// When setup is not required, allow a 30-second timeout. If a host device is on
// a slow data connection, enabling the tether hotspot may take a significant
// amount of time because most phones must send a "provisioning" request to
// the mobile provider to ask the provider whether tethering is allowed.
// static
const uint32_t
    ConnectTetheringOperation::kSetupNotRequiredResponseTimeoutSeconds = 30;

// When setup is required, the timeout is extended another 90 seconds because
// setup requires that the user interact with a notification on the Tether host.
// static
const uint32_t ConnectTetheringOperation::kSetupRequiredResponseTimeoutSeconds =
    120;

// static
ConnectTetheringOperation::Factory*
    ConnectTetheringOperation::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<ConnectTetheringOperation>
ConnectTetheringOperation::Factory::NewInstance(
    const cryptauth::RemoteDevice& device_to_connect,
    BleConnectionManager* connection_manager,
    TetherHostResponseRecorder* tether_host_response_recorder,
    bool setup_required) {
  if (!factory_instance_) {
    factory_instance_ = new Factory();
  }
  return factory_instance_->BuildInstance(device_to_connect, connection_manager,
                                          tether_host_response_recorder,
                                          setup_required);
}

// static
void ConnectTetheringOperation::Factory::SetInstanceForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

std::unique_ptr<ConnectTetheringOperation>
ConnectTetheringOperation::Factory::BuildInstance(
    const cryptauth::RemoteDevice& device_to_connect,
    BleConnectionManager* connection_manager,
    TetherHostResponseRecorder* tether_host_response_recorder,
    bool setup_required) {
  return base::WrapUnique(new ConnectTetheringOperation(
      device_to_connect, connection_manager, tether_host_response_recorder,
      setup_required));
}

ConnectTetheringOperation::ConnectTetheringOperation(
    const cryptauth::RemoteDevice& device_to_connect,
    BleConnectionManager* connection_manager,
    TetherHostResponseRecorder* tether_host_response_recorder,
    bool setup_required)
    : MessageTransferOperation(
          std::vector<cryptauth::RemoteDevice>{device_to_connect},
          connection_manager),
      remote_device_(device_to_connect),
      tether_host_response_recorder_(tether_host_response_recorder),
      clock_(base::DefaultClock::GetInstance()),
      setup_required_(setup_required),
      error_code_to_return_(
          ConnectTetheringResponse_ResponseCode::
              ConnectTetheringResponse_ResponseCode_UNKNOWN_ERROR) {}

ConnectTetheringOperation::~ConnectTetheringOperation() = default;

void ConnectTetheringOperation::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ConnectTetheringOperation::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ConnectTetheringOperation::OnDeviceAuthenticated(
    const cryptauth::RemoteDevice& remote_device) {
  DCHECK(remote_devices().size() == 1u && remote_devices()[0] == remote_device);
  connect_tethering_request_start_time_ = clock_->Now();
  connect_message_sequence_number_ = SendMessageToDevice(
      remote_device,
      std::make_unique<MessageWrapper>(ConnectTetheringRequest()));
}

void ConnectTetheringOperation::OnMessageReceived(
    std::unique_ptr<MessageWrapper> message_wrapper,
    const cryptauth::RemoteDevice& remote_device) {
  if (message_wrapper->GetMessageType() !=
      MessageType::CONNECT_TETHERING_RESPONSE) {
    // If another type of message has been received, ignore it.
    return;
  }

  if (!(remote_device == remote_device_)) {
    // If the message came from another device, ignore it.
    return;
  }

  ConnectTetheringResponse* response =
      static_cast<ConnectTetheringResponse*>(message_wrapper->GetProto().get());
  if (response->response_code() ==
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_SUCCESS) {
    if (response->has_ssid() && response->has_password()) {
      PA_LOG(INFO) << "Received ConnectTetheringResponse from device with ID "
                   << remote_device.GetTruncatedDeviceIdForLogs() << " and "
                   << "response_code == SUCCESS. Config: {ssid: \""
                   << response->ssid() << "\", password: \""
                   << response->password() << "\"}";

      tether_host_response_recorder_->RecordSuccessfulConnectTetheringResponse(
          remote_device);

      // Save the response values here, but do not notify observers until
      // OnOperationFinished(). Notifying observers at this point can cause this
      // object to be deleted, resulting in a crash.
      ssid_to_return_ = response->ssid();
      password_to_return_ = response->password();
    } else {
      PA_LOG(ERROR) << "Received ConnectTetheringResponse from device with ID "
                    << remote_device.GetTruncatedDeviceIdForLogs() << " and "
                    << "response_code == SUCCESS, but the response did not "
                    << "contain a Wi-Fi SSID and/or password.";
    }
  } else {
    PA_LOG(INFO) << "Received ConnectTetheringResponse from device with ID "
                 << remote_device.GetTruncatedDeviceIdForLogs() << " and "
                 << "response_code == " << response->response_code() << ".";
    error_code_to_return_ = response->response_code();
  }

  // UMA_HISTOGRAM_MEDIUM_TIMES is used because UMA_HISTOGRAM_TIMES has a max
  // of 10 seconds, and it can take up to 90 seconds for a
  // ConnectTetheringResponse.
  DCHECK(!connect_tethering_request_start_time_.is_null());
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "InstantTethering.Performance.ConnectTetheringResponseDuration",
      clock_->Now() - connect_tethering_request_start_time_);

  // Now that a response has been received, the device can be unregistered.
  UnregisterDevice(remote_device);
}

void ConnectTetheringOperation::OnOperationFinished() {
  // If |ssid_to_return_| has not been set, either the operation finished with a
  // failed response or no connection succeeded at all. In these cases, notify
  // observers of a failure.
  if (ssid_to_return_.empty()) {
    NotifyObserversOfConnectionFailure(error_code_to_return_);
    return;
  }

  NotifyObserversOfSuccessfulResponse(ssid_to_return_, password_to_return_);
}

MessageType ConnectTetheringOperation::GetMessageTypeForConnection() {
  return MessageType::CONNECT_TETHERING_REQUEST;
}

void ConnectTetheringOperation::OnMessageSent(int sequence_number) {
  if (sequence_number != connect_message_sequence_number_)
    return;

  NotifyConnectTetheringRequestSent();
}

void ConnectTetheringOperation::NotifyConnectTetheringRequestSent() {
  for (auto& observer : observer_list_)
    observer.OnConnectTetheringRequestSent(remote_device_);
}

void ConnectTetheringOperation::NotifyObserversOfSuccessfulResponse(
    const std::string& ssid,
    const std::string& password) {
  for (auto& observer : observer_list_) {
    observer.OnSuccessfulConnectTetheringResponse(remote_device_, ssid,
                                                  password);
  }
}

void ConnectTetheringOperation::NotifyObserversOfConnectionFailure(
    ConnectTetheringResponse_ResponseCode error_code) {
  for (auto& observer : observer_list_)
    observer.OnConnectTetheringFailure(remote_device_, error_code);
}

uint32_t ConnectTetheringOperation::GetTimeoutSeconds() {
  return setup_required_
             ? ConnectTetheringOperation::kSetupRequiredResponseTimeoutSeconds
             : ConnectTetheringOperation::
                   kSetupNotRequiredResponseTimeoutSeconds;
}

void ConnectTetheringOperation::SetClockForTest(base::Clock* clock_for_test) {
  clock_ = clock_for_test;
}

}  // namespace tether

}  // namespace chromeos
