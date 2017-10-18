// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scanner_operation.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "chromeos/components/tether/host_scan_device_prioritizer.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "chromeos/components/tether/tether_host_response_recorder.h"
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
    HostScanDevicePrioritizer* host_scan_device_prioritizer,
    TetherHostResponseRecorder* tether_host_response_recorder) {
  if (!factory_instance_) {
    factory_instance_ = new Factory();
  }
  return factory_instance_->BuildInstance(
      devices_to_connect, connection_manager, host_scan_device_prioritizer,
      tether_host_response_recorder);
}

// static
void HostScannerOperation::Factory::SetInstanceForTesting(Factory* factory) {
  factory_instance_ = factory;
}

std::unique_ptr<HostScannerOperation>
HostScannerOperation::Factory::BuildInstance(
    const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
    BleConnectionManager* connection_manager,
    HostScanDevicePrioritizer* host_scan_device_prioritizer,
    TetherHostResponseRecorder* tether_host_response_recorder) {
  return base::MakeUnique<HostScannerOperation>(
      devices_to_connect, connection_manager, host_scan_device_prioritizer,
      tether_host_response_recorder);
}

HostScannerOperation::ScannedDeviceInfo::ScannedDeviceInfo(
    const cryptauth::RemoteDevice& remote_device,
    const DeviceStatus& device_status,
    bool setup_required)
    : remote_device(remote_device),
      device_status(device_status),
      setup_required(setup_required) {}

HostScannerOperation::ScannedDeviceInfo::~ScannedDeviceInfo() {}

bool operator==(const HostScannerOperation::ScannedDeviceInfo& first,
                const HostScannerOperation::ScannedDeviceInfo& second) {
  return first.remote_device == second.remote_device &&
         first.device_status.SerializeAsString() ==
             second.device_status.SerializeAsString() &&
         first.setup_required == second.setup_required;
}

HostScannerOperation::HostScannerOperation(
    const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
    BleConnectionManager* connection_manager,
    HostScanDevicePrioritizer* host_scan_device_prioritizer,
    TetherHostResponseRecorder* tether_host_response_recorder)
    : MessageTransferOperation(
          PrioritizeDevices(devices_to_connect, host_scan_device_prioritizer),
          connection_manager),
      tether_host_response_recorder_(tether_host_response_recorder),
      clock_(base::MakeUnique<base::DefaultClock>()) {}

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
  DCHECK(!base::ContainsKey(
      device_id_to_tether_availability_request_start_time_map_,
      remote_device.GetDeviceId()));
  device_id_to_tether_availability_request_start_time_map_[remote_device
                                                               .GetDeviceId()] =
      clock_->Now();
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
    bool setup_required =
        response->response_code() ==
        TetherAvailabilityResponse_ResponseCode::
            TetherAvailabilityResponse_ResponseCode_SETUP_NEEDED;

    PA_LOG(INFO) << "Received TetherAvailabilityResponse from device with ID "
                 << remote_device.GetTruncatedDeviceIdForLogs() << " which "
                 << "indicates that tethering is available. setup_required = "
                 << setup_required;

    tether_host_response_recorder_->RecordSuccessfulTetherAvailabilityResponse(
        remote_device);

    scanned_device_list_so_far_.push_back(ScannedDeviceInfo(
        remote_device, response->device_status(), setup_required));
    NotifyObserversOfScannedDeviceList(false /* is_final_scan_result */);
  }

  RecordTetherAvailabilityResponseDuration(remote_device.GetDeviceId());

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

void HostScannerOperation::SetClockForTest(
    std::unique_ptr<base::Clock> clock_for_test) {
  clock_ = std::move(clock_for_test);
}

void HostScannerOperation::RecordTetherAvailabilityResponseDuration(
    const std::string device_id) {
  if (!base::ContainsKey(
          device_id_to_tether_availability_request_start_time_map_,
          device_id) ||
      device_id_to_tether_availability_request_start_time_map_[device_id]
          .is_null()) {
    LOG(ERROR) << "Failed to record TetherAvailabilityResponse duration: "
               << "start time is invalid";
    return;
  }

  UMA_HISTOGRAM_TIMES(
      "InstantTethering.Performance.TetherAvailabilityResponseDuration",
      clock_->Now() -
          device_id_to_tether_availability_request_start_time_map_[device_id]);
  device_id_to_tether_availability_request_start_time_map_.erase(device_id);
}

}  // namespace tether

}  // namespace chromeos
