// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scanner_operation.h"

#include "chromeos/components/tether/host_scan_device_prioritizer.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

namespace {

std::vector<cryptauth::RemoteDevice> PrioritizeDevices(
    const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
    HostScanDevicePrioritizer* host_scan_device_prioritizer) {
  std::vector<cryptauth::RemoteDevice> mutable_devices_to_connect =
      devices_to_connect;
  host_scan_device_prioritizer->SortByHostScanOrder(
      &mutable_devices_to_connect);
  return mutable_devices_to_connect;
}

bool IsTetheringAvailableWithValidDeviceStatus(
    const TetherAvailabilityResponse* response) {
  if (!response) {
    return false;
  }

  if (!response->has_device_status()) {
    return false;
  }

  if (!response->has_response_code()) {
    return false;
  }

  const TetherAvailabilityResponse_ResponseCode response_code =
      response->response_code();
  if (response_code ==
          TetherAvailabilityResponse_ResponseCode::
              TetherAvailabilityResponse_ResponseCode_SETUP_NEEDED ||
      response_code ==
          TetherAvailabilityResponse_ResponseCode::
              TetherAvailabilityResponse_ResponseCode_TETHER_AVAILABLE) {
    return true;
  }

  return false;
}

}  // namespace

// static
HostScannerOperation::Factory*
    HostScannerOperation::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<HostScannerOperation>
HostScannerOperation::Factory::NewInstance(
    const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
    BleConnectionManager* connection_manager,
    HostScanDevicePrioritizer* host_scan_device_prioritizer) {
  if (!factory_instance_) {
    factory_instance_ = new Factory();
  }
  return factory_instance_->BuildInstance(
      devices_to_connect, connection_manager, host_scan_device_prioritizer);
}

// static
void HostScannerOperation::Factory::SetInstanceForTesting(Factory* factory) {
  factory_instance_ = factory;
}

std::unique_ptr<HostScannerOperation>
HostScannerOperation::Factory::BuildInstance(
    const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
    BleConnectionManager* connection_manager,
    HostScanDevicePrioritizer* host_scan_device_prioritizer) {
  return base::MakeUnique<HostScannerOperation>(
      devices_to_connect, connection_manager, host_scan_device_prioritizer);
}

HostScannerOperation::ScannedDeviceInfo::ScannedDeviceInfo(
    const cryptauth::RemoteDevice& remote_device,
    const DeviceStatus& device_status,
    bool set_up_required)
    : remote_device(remote_device),
      device_status(device_status),
      set_up_required(set_up_required) {}

HostScannerOperation::ScannedDeviceInfo::~ScannedDeviceInfo() {}

bool operator==(const HostScannerOperation::ScannedDeviceInfo& first,
                const HostScannerOperation::ScannedDeviceInfo& second) {
  return first.remote_device == second.remote_device &&
         first.device_status.SerializeAsString() ==
             second.device_status.SerializeAsString() &&
         first.set_up_required == second.set_up_required;
}

HostScannerOperation::HostScannerOperation(
    const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
    BleConnectionManager* connection_manager,
    HostScanDevicePrioritizer* host_scan_device_prioritizer)
    : MessageTransferOperation(
          PrioritizeDevices(devices_to_connect, host_scan_device_prioritizer),
          connection_manager),
      host_scan_device_prioritizer_(host_scan_device_prioritizer) {}

HostScannerOperation::~HostScannerOperation() {}

void HostScannerOperation::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HostScannerOperation::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void HostScannerOperation::NotifyObserversOfScannedDeviceList(
    bool is_final_scan_result) {
  for (auto& observer : observer_list_) {
    observer.OnTetherAvailabilityResponse(scanned_device_list_so_far_,
                                          is_final_scan_result);
  }
}

void HostScannerOperation::OnDeviceAuthenticated(
    const cryptauth::RemoteDevice& remote_device) {
  SendMessageToDevice(remote_device, base::MakeUnique<MessageWrapper>(
                                         TetherAvailabilityRequest()));
}

void HostScannerOperation::OnMessageReceived(
    std::unique_ptr<MessageWrapper> message_wrapper,
    const cryptauth::RemoteDevice& remote_device) {
  if (message_wrapper->GetMessageType() !=
      MessageType::TETHER_AVAILABILITY_RESPONSE) {
    // If another type of message has been received, ignore it.
    return;
  }

  TetherAvailabilityResponse* response =
      static_cast<TetherAvailabilityResponse*>(
          message_wrapper->GetProto().get());
  if (!IsTetheringAvailableWithValidDeviceStatus(response)) {
    // If the received message is invalid or if it states that tethering is
    // unavailable, ignore it.
    PA_LOG(INFO) << "Received TetherAvailabilityResponse from device with ID "
                 << remote_device.GetTruncatedDeviceIdForLogs() << " which "
                 << "indicates that tethering is not available.";
  } else {
    bool set_up_required =
        response->response_code() ==
        TetherAvailabilityResponse_ResponseCode::
            TetherAvailabilityResponse_ResponseCode_SETUP_NEEDED;

    PA_LOG(INFO) << "Received TetherAvailabilityResponse from device with ID "
                 << remote_device.GetTruncatedDeviceIdForLogs() << " which "
                 << "indicates that tethering is available. set_up_required = "
                 << set_up_required;

    host_scan_device_prioritizer_->RecordSuccessfulTetherAvailabilityResponse(
        remote_device);

    scanned_device_list_so_far_.push_back(ScannedDeviceInfo(
        remote_device, response->device_status(), set_up_required));
    NotifyObserversOfScannedDeviceList(false /* is_final_scan_result */);
  }

  // Unregister the device after a TetherAvailabilityResponse has been received.
  UnregisterDevice(remote_device);
}

void HostScannerOperation::OnOperationStarted() {
  DCHECK(scanned_device_list_so_far_.empty());

  // Send out an empty device list scan to let observers know that the operation
  // has begun.
  NotifyObserversOfScannedDeviceList(false /* is_final_scan_result */);
}

void HostScannerOperation::OnOperationFinished() {
  NotifyObserversOfScannedDeviceList(true /* is_final_scan_result */);
}

MessageType HostScannerOperation::GetMessageTypeForConnection() {
  return MessageType::TETHER_AVAILABILITY_REQUEST;
}

}  // namespace tether

}  // namespace chromeos
