// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_scanner.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chromeos/components/tether/ble_constants.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/remote_device.h"
#include "components/proximity_auth/logging/logging.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace chromeos {

namespace tether {

namespace {

// Minimum RSSI value to use for discovery. The -90 value was determined
// empirically and is borrowed from
// |proximity_auth::BluetoothLowEnergyConnectionFinder|.
const int kMinDiscoveryRSSI = -90;

// Valid service data must include at least 4 bytes: 2 bytes associated with the
// scanning device (used as a scan filter) and 2 bytes which identify the
// advertising device to the scanning device.
const size_t kMinNumBytesInServiceData = 4;

}  // namespace

BleScanner::ServiceDataProviderImpl::ServiceDataProviderImpl() {}

BleScanner::ServiceDataProviderImpl::~ServiceDataProviderImpl() {}

const std::vector<uint8_t>*
BleScanner::ServiceDataProviderImpl::GetServiceDataForUUID(
    device::BluetoothDevice* bluetooth_device) {
  return bluetooth_device->GetServiceDataForUUID(
      device::BluetoothUUID(kAdvertisingServiceUuid));
}

BleScanner::BleScanner(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const LocalDeviceDataProvider* local_device_data_provider)
    : BleScanner(base::MakeUnique<ServiceDataProviderImpl>(),
                 adapter,
                 base::WrapUnique(new cryptauth::ForegroundEidGenerator()),
                 local_device_data_provider) {}

BleScanner::BleScanner(
    std::unique_ptr<ServiceDataProvider> service_data_provider,
    scoped_refptr<device::BluetoothAdapter> adapter,
    std::unique_ptr<cryptauth::ForegroundEidGenerator> eid_generator,
    const LocalDeviceDataProvider* local_device_data_provider)
    : service_data_provider_(std::move(service_data_provider)),
      adapter_(adapter),
      eid_generator_(std::move(eid_generator)),
      local_device_data_provider_(local_device_data_provider),
      is_initializing_discovery_session_(false),
      discovery_session_(nullptr),
      weak_ptr_factory_(this) {
  adapter_->AddObserver(this);
}

BleScanner::~BleScanner() {
  adapter_->RemoveObserver(this);
}

bool BleScanner::RegisterScanFilterForDevice(
    const cryptauth::RemoteDevice& remote_device) {
  if (registered_remote_devices_.size() >= kMaxConcurrentAdvertisements) {
    // Each scan filter corresponds to an advertisement. Thus, the number of
    // concurrent advertisements cannot exceed the maximum number of concurrent
    // advertisements.
    PA_LOG(WARNING) << "Attempted to start a scan for a new device when the "
                    << "maximum number of devices have already been "
                    << "registered.";
    return false;
  }

  std::vector<cryptauth::BeaconSeed> local_device_beacon_seeds;
  if (!local_device_data_provider_->GetLocalDeviceData(
          nullptr, &local_device_beacon_seeds)) {
    PA_LOG(WARNING) << "Error fetching the local device's beacon seeds. Cannot "
                    << "generate scan without beacon seeds.";
    return false;
  }

  std::unique_ptr<cryptauth::ForegroundEidGenerator::EidData> scan_filters =
      eid_generator_->GenerateBackgroundScanFilter(local_device_beacon_seeds);
  if (!scan_filters) {
    PA_LOG(WARNING) << "Error generating background scan filters. Cannot scan";
    return false;
  }

  registered_remote_devices_.push_back(remote_device);
  UpdateDiscoveryStatus();

  return true;
}

bool BleScanner::UnregisterScanFilterForDevice(
    const cryptauth::RemoteDevice& remote_device) {
  for (auto it = registered_remote_devices_.begin();
       it != registered_remote_devices_.end(); ++it) {
    if (it->GetDeviceId() == remote_device.GetDeviceId()) {
      registered_remote_devices_.erase(it);
      UpdateDiscoveryStatus();
      return true;
    }
  }

  return false;
}

bool BleScanner::IsDeviceRegistered(const std::string& device_id) {
  for (auto it = registered_remote_devices_.begin();
       it != registered_remote_devices_.end(); ++it) {
    if (it->GetDeviceId() == device_id) {
      return true;
    }
  }

  return false;
}

void BleScanner::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void BleScanner::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void BleScanner::AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                       bool powered) {
  DCHECK_EQ(adapter_.get(), adapter);
  UpdateDiscoveryStatus();
}

void BleScanner::DeviceAdded(device::BluetoothAdapter* adapter,
                             device::BluetoothDevice* bluetooth_device) {
  DCHECK_EQ(adapter_.get(), adapter);
  HandleDeviceUpdated(bluetooth_device);
}

void BleScanner::DeviceChanged(device::BluetoothAdapter* adapter,
                               device::BluetoothDevice* bluetooth_device) {
  DCHECK_EQ(adapter_.get(), adapter);
  HandleDeviceUpdated(bluetooth_device);
}

void BleScanner::UpdateDiscoveryStatus() {
  if (registered_remote_devices_.empty()) {
    StopDiscoverySession();
    return;
  }

  if (!adapter_->IsPowered()) {
    // If the adapter has powered off, no devices can be discovered.
    StopDiscoverySession();
    return;
  }

  if (is_initializing_discovery_session_) {
    return;
  } else if (!discovery_session_ ||
             (discovery_session_ && !discovery_session_->IsActive())) {
    StartDiscoverySession();
  }
}

void BleScanner::StartDiscoverySession() {
  DCHECK(adapter_);
  is_initializing_discovery_session_ = true;

  // Discover only low energy (LE) devices with strong enough signal.
  std::unique_ptr<device::BluetoothDiscoveryFilter> filter =
      base::MakeUnique<device::BluetoothDiscoveryFilter>(
          device::BLUETOOTH_TRANSPORT_LE);
  filter->SetRSSI(kMinDiscoveryRSSI);

  adapter_->StartDiscoverySessionWithFilter(
      std::move(filter), base::Bind(&BleScanner::OnDiscoverySessionStarted,
                                    weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BleScanner::OnStartDiscoverySessionError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BleScanner::OnDiscoverySessionStarted(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  is_initializing_discovery_session_ = false;
  discovery_session_ = std::move(discovery_session);
}

void BleScanner::OnStartDiscoverySessionError() {
  PA_LOG(WARNING) << "Error starting discovery session. Initialization failed.";
  is_initializing_discovery_session_ = false;
}

void BleScanner::StopDiscoverySession() {
  if (!discovery_session_) {
    // If there is no discovery session to stop, return early.
    return;
  }

  discovery_session_.reset();
}

void BleScanner::HandleDeviceUpdated(
    device::BluetoothDevice* bluetooth_device) {
  DCHECK(bluetooth_device);

  const std::vector<uint8_t>* service_data =
      service_data_provider_->GetServiceDataForUUID(bluetooth_device);
  if (!service_data || service_data->size() < kMinNumBytesInServiceData) {
    // If there is no service data or the service data is of insufficient
    // length, there is not enough information to create a connection.
    return;
  }

  // Convert the service data from a std::vector<uint8_t> to a std::string.
  std::string service_data_str;
  char* string_contents_ptr =
      base::WriteInto(&service_data_str, service_data->size() + 1);
  memcpy(string_contents_ptr, service_data->data(), service_data->size());

  CheckForMatchingScanFilters(bluetooth_device, service_data_str);
}

void BleScanner::CheckForMatchingScanFilters(
    device::BluetoothDevice* bluetooth_device,
    std::string& service_data) {
  std::vector<cryptauth::BeaconSeed> beacon_seeds;
  if (!local_device_data_provider_->GetLocalDeviceData(nullptr,
                                                       &beacon_seeds)) {
    // If no beacon seeds are available, the scan cannot be checked for a match.
    return;
  }

  const cryptauth::RemoteDevice* identified_device =
      eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          service_data, registered_remote_devices_, beacon_seeds);

  // If the service data does not correspond to an advertisement from a device
  // on this account, ignore it.
  if (!identified_device)
    return;

  for (auto& observer : observer_list_) {
    observer.OnReceivedAdvertisementFromDevice(bluetooth_device->GetAddress(),
                                               *identified_device);
  }
}

}  // namespace tether

}  // namespace chromeos
