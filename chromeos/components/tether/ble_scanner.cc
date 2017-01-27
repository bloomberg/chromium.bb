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

// Returns out |string|s data as a hex string.
std::string StringToHexOfContents(const std::string& string) {
  std::stringstream ss;
  ss << "0x" << std::hex;

  for (size_t i = 0; i < string.size(); i++) {
    ss << static_cast<int>(string.data()[i]);
  }

  return ss.str();
}

}  // namespace

BleScanner::DelegateImpl::DelegateImpl() {}

BleScanner::DelegateImpl::~DelegateImpl() {}

bool BleScanner::DelegateImpl::IsBluetoothAdapterAvailable() const {
  return device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable();
}

void BleScanner::DelegateImpl::GetAdapter(
    const device::BluetoothAdapterFactory::AdapterCallback& callback) {
  device::BluetoothAdapterFactory::GetAdapter(callback);
}

const std::vector<uint8_t>* BleScanner::DelegateImpl::GetServiceDataForUUID(
    const device::BluetoothUUID& service_uuid,
    device::BluetoothDevice* bluetooth_device) {
  return bluetooth_device->GetServiceDataForUUID(service_uuid);
}

BleScanner::BleScanner(
    const LocalDeviceDataProvider* local_device_data_provider)
    : BleScanner(base::MakeUnique<DelegateImpl>(),
                 cryptauth::EidGenerator::GetInstance(),
                 local_device_data_provider) {}

BleScanner::~BleScanner() {}

BleScanner::BleScanner(
    std::unique_ptr<Delegate> delegate,
    const cryptauth::EidGenerator* eid_generator,
    const LocalDeviceDataProvider* local_device_data_provider)
    : delegate_(std::move(delegate)),
      eid_generator_(eid_generator),
      local_device_data_provider_(local_device_data_provider),
      is_initializing_adapter_(false),
      is_initializing_discovery_session_(false),
      discovery_session_(nullptr),
      weak_ptr_factory_(this) {}

bool BleScanner::RegisterScanFilterForDevice(
    const cryptauth::RemoteDevice& remote_device) {
  if (!delegate_->IsBluetoothAdapterAvailable()) {
    PA_LOG(ERROR) << "Bluetooth is not supported on this platform.";
    return false;
  }

  if (registered_remote_devices_.size() >= kMaxConcurrentAdvertisements) {
    // Each scan filter corresponds to an advertisement. Thus, the number of
    // concurrent advertisements cannot exceed the maximum number of concurrent
    // advertisements.
    return false;
  }

  std::vector<cryptauth::BeaconSeed> local_device_beacon_seeds;
  if (!local_device_data_provider_->GetLocalDeviceData(
          nullptr, &local_device_beacon_seeds)) {
    // If the local device's beacon seeds could not be fetched, a scan filter
    // cannot be generated.
    return false;
  }

  std::unique_ptr<cryptauth::EidGenerator::EidData> scan_filters =
      eid_generator_->GenerateBackgroundScanFilter(local_device_beacon_seeds);
  if (!scan_filters) {
    // If a background scan filter cannot be generated, give up.
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
  PA_LOG(INFO) << "Adapter power changed. Powered = " << powered;
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

  if (is_initializing_adapter_) {
    return;
  } else if (!adapter_) {
    InitializeBluetoothAdapter();
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

void BleScanner::InitializeBluetoothAdapter() {
  PA_LOG(INFO) << "Initializing Bluetooth adapter.";
  is_initializing_adapter_ = true;
  delegate_->GetAdapter(base::Bind(&BleScanner::OnAdapterInitialized,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void BleScanner::OnAdapterInitialized(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK(is_initializing_adapter_ && !discovery_session_ &&
         !is_initializing_discovery_session_);
  PA_LOG(INFO) << "Bluetooth adapter initialized.";
  is_initializing_adapter_ = false;

  adapter_ = adapter;
  adapter_->AddObserver(this);

  UpdateDiscoveryStatus();
}

void BleScanner::StartDiscoverySession() {
  DCHECK(adapter_);
  PA_LOG(INFO) << "Starting discovery session.";
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
  PA_LOG(INFO) << "Discovery session started. Scanning fully initialized.";
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

  PA_LOG(WARNING) << "Stopping discovery session.";
  discovery_session_.reset();
}

void BleScanner::HandleDeviceUpdated(
    device::BluetoothDevice* bluetooth_device) {
  DCHECK(bluetooth_device);

  const std::vector<uint8_t>* service_data = delegate_->GetServiceDataForUUID(
      device::BluetoothUUID(kAdvertisingServiceUuid), bluetooth_device);
  if (!service_data || service_data->size() < kMinNumBytesInServiceData) {
    // If there is no service data or the service data is of insufficient
    // length, there is not enough information to create a connection.
    return;
  }

  // Convert the service data from a std::vector<uint8_t> to a std::string.
  std::string service_data_str;
  char* string_contents_ptr =
      base::WriteInto(&service_data_str, service_data->size() + 1);
  memcpy(string_contents_ptr, service_data->data(), service_data->size() + 1);

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

  if (identified_device) {
    PA_LOG(INFO) << "Received advertisement from remote device with ID "
                 << identified_device->GetTruncatedDeviceIdForLogs() << ".";
    for (auto& observer : observer_list_) {
      observer.OnReceivedAdvertisementFromDevice(bluetooth_device->GetAddress(),
                                                 *identified_device);
    }
  } else {
    PA_LOG(INFO) << "Received advertisement remote device, but could not "
                 << "identify the device. Service data: "
                 << StringToHexOfContents(service_data) << ".";
  }
}

}  // namespace tether

}  // namespace chromeos
