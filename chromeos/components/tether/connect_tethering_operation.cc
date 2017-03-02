// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/connect_tethering_operation.h"

#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

// static
ConnectTetheringOperation::Factory*
    ConnectTetheringOperation::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<ConnectTetheringOperation>
ConnectTetheringOperation::Factory::NewInstance(
    const cryptauth::RemoteDevice& device_to_connect,
    BleConnectionManager* connection_manager) {
  if (!factory_instance_) {
    factory_instance_ = new Factory();
  }
  return factory_instance_->BuildInstance(device_to_connect,
                                          connection_manager);
}

// static
void ConnectTetheringOperation::Factory::SetInstanceForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

std::unique_ptr<ConnectTetheringOperation>
ConnectTetheringOperation::Factory::BuildInstance(
    const cryptauth::RemoteDevice& device_to_connect,
    BleConnectionManager* connection_manager) {
  return base::MakeUnique<ConnectTetheringOperation>(device_to_connect,
                                                     connection_manager);
}

ConnectTetheringOperation::ConnectTetheringOperation(
    const cryptauth::RemoteDevice& device_to_connect,
    BleConnectionManager* connection_manager)
    : MessageTransferOperation(
          std::vector<cryptauth::RemoteDevice>{device_to_connect},
          connection_manager),
      has_authenticated_(false) {}

ConnectTetheringOperation::~ConnectTetheringOperation() {}

void ConnectTetheringOperation::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ConnectTetheringOperation::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ConnectTetheringOperation::OnDeviceAuthenticated(
    const cryptauth::RemoteDevice& remote_device) {
  DCHECK(remote_devices().size() == 1u && remote_devices()[0] == remote_device);
  has_authenticated_ = true;

  SendMessageToDevice(remote_device, base::MakeUnique<MessageWrapper>(
                                         ConnectTetheringRequest()));
}

void ConnectTetheringOperation::OnMessageReceived(
    std::unique_ptr<MessageWrapper> message_wrapper,
    const cryptauth::RemoteDevice& remote_device) {
  if (message_wrapper->GetMessageType() !=
      MessageType::CONNECT_TETHERING_RESPONSE) {
    // If another type of message has been received, ignore it.
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
      NotifyObserversOfSuccessfulResponse(response->ssid(),
                                          response->password());
    } else {
      PA_LOG(ERROR) << "Received ConnectTetheringResponse from device with ID "
                    << remote_device.GetTruncatedDeviceIdForLogs() << " and "
                    << "response_code == SUCCESS, but the response did not "
                    << "contain a Wi-Fi SSID and/or password.";
      NotifyObserversOfConnectionFailure(
          ConnectTetheringResponse_ResponseCode::
              ConnectTetheringResponse_ResponseCode_UNKNOWN_ERROR);
    }
  } else {
    PA_LOG(INFO) << "Received ConnectTetheringResponse from device with ID "
                 << remote_device.GetTruncatedDeviceIdForLogs() << " and "
                 << "response_code == " << response->response_code() << ".";
    NotifyObserversOfConnectionFailure(response->response_code());
  }

  // Now that a response has been received, the device can be unregistered.
  UnregisterDevice(remote_device);
}

void ConnectTetheringOperation::OnOperationFinished() {
  if (!has_authenticated_) {
    // If the operation finished but the device never authenticated, there was
    // some sort of problem connecting to the device. In this case, notify
    // observers of a failure.
    NotifyObserversOfConnectionFailure(
        ConnectTetheringResponse_ResponseCode::
            ConnectTetheringResponse_ResponseCode_UNKNOWN_ERROR);
  }
}

MessageType ConnectTetheringOperation::GetMessageTypeForConnection() {
  return MessageType::CONNECT_TETHERING_REQUEST;
}

void ConnectTetheringOperation::NotifyObserversOfSuccessfulResponse(
    const std::string& ssid,
    const std::string& password) {
  for (auto& observer : observer_list_) {
    observer.OnSuccessfulConnectTetheringResponse(ssid, password);
  }
}

void ConnectTetheringOperation::NotifyObserversOfConnectionFailure(
    ConnectTetheringResponse_ResponseCode error_code) {
  for (auto& observer : observer_list_) {
    observer.OnConnectTetheringFailure(error_code);
  }
}

}  // namespace tether

}  // namespace chromeos
