// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_advertiser.h"

#include "base/bind.h"
#include "chromeos/components/tether/ble_constants.h"
#include "chromeos/components/tether/local_device_data_provider.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/remote_beacon_seed_fetcher.h"
#include "components/proximity_auth/logging/logging.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace chromeos {

namespace tether {

namespace {

uint8_t kInvertedConnectionFlag = 0x01;

}  // namespace

BleAdvertiser::IndividualAdvertisement::IndividualAdvertisement(
    const std::string& device_id,
    scoped_refptr<device::BluetoothAdapter> adapter,
    std::unique_ptr<cryptauth::DataWithTimestamp> advertisement_data)
    : device_id_(device_id),
      adapter_(adapter),
      advertisement_data_(std::move(advertisement_data)),
      is_initializing_advertising_(false),
      advertisement_(nullptr),
      weak_ptr_factory_(this) {
  adapter_->AddObserver(this);
  AdvertiseIfPossible();
}

BleAdvertiser::IndividualAdvertisement::~IndividualAdvertisement() {
  if (advertisement_) {
    advertisement_->Unregister(
        base::Bind(&base::DoNothing),
        base::Bind(&IndividualAdvertisement::OnAdvertisementUnregisterFailure,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  adapter_->RemoveObserver(this);
}

void BleAdvertiser::IndividualAdvertisement::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  DCHECK(adapter_.get() == adapter);
  AdvertiseIfPossible();
}

void BleAdvertiser::IndividualAdvertisement::AdvertisementReleased(
    device::BluetoothAdvertisement* advertisement) {
  DCHECK(advertisement_.get() == advertisement);

  // If the advertisement was released, delete it and try again. Note that this
  // situation is not expected to occur under normal circumstances.
  advertisement_ = nullptr;
  AdvertiseIfPossible();
}

void BleAdvertiser::IndividualAdvertisement::AdvertiseIfPossible() {
  if (!adapter_->IsPowered() || is_initializing_advertising_ ||
      advertisement_) {
    return;
  }

  is_initializing_advertising_ = true;

  std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data =
      base::MakeUnique<device::BluetoothAdvertisement::Data>(
          device::BluetoothAdvertisement::AdvertisementType::
              ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_service_uuids(CreateServiceUuids());
  advertisement_data->set_service_data(CreateServiceData());

  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      base::Bind(&IndividualAdvertisement::OnAdvertisementRegisteredCallback,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&IndividualAdvertisement::OnAdvertisementErrorCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BleAdvertiser::IndividualAdvertisement::OnAdvertisementRegisteredCallback(
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  is_initializing_advertising_ = false;
  advertisement_ = advertisement;
  PA_LOG(INFO) << "Advertisement registered. "
               << "Device ID: \""
               << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(device_id_)
               << "\", Service data: " << advertisement_data_->DataInHex();
}

void BleAdvertiser::IndividualAdvertisement::OnAdvertisementErrorCallback(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  is_initializing_advertising_ = false;
  PA_LOG(ERROR) << "Error registering advertisement. "
                << "Device ID: \""
                << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(device_id_)
                << "\", Service data: " << advertisement_data_->DataInHex()
                << ", Error code: " << error_code;
}

void BleAdvertiser::IndividualAdvertisement::OnAdvertisementUnregisterFailure(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  PA_LOG(ERROR) << "Error unregistering advertisement. "
                << "Device ID: \""
                << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(device_id_)
                << "\", Service data: " << advertisement_data_->DataInHex()
                << " Error code: " << error_code;
}

std::unique_ptr<device::BluetoothAdvertisement::UUIDList>
BleAdvertiser::IndividualAdvertisement::CreateServiceUuids() const {
  std::unique_ptr<device::BluetoothAdvertisement::UUIDList> list =
      base::MakeUnique<device::BluetoothAdvertisement::UUIDList>();
  list->push_back(std::string(kAdvertisingServiceUuid));
  return list;
}

std::unique_ptr<device::BluetoothAdvertisement::ServiceData>
BleAdvertiser::IndividualAdvertisement::CreateServiceData() const {
  DCHECK(!advertisement_data_->data.empty());

  std::vector<uint8_t> data_as_vector(advertisement_data_->data.size());
  memcpy(data_as_vector.data(), advertisement_data_->data.data(),
         advertisement_data_->data.size());

  // Add a flag at the end of the service data to signify that the inverted
  // connection flow should be used.
  data_as_vector.push_back(kInvertedConnectionFlag);

  std::unique_ptr<device::BluetoothAdvertisement::ServiceData> service_data =
      base::MakeUnique<device::BluetoothAdvertisement::ServiceData>();
  service_data->insert(std::pair<std::string, std::vector<uint8_t>>(
      std::string(kAdvertisingServiceUuid), data_as_vector));
  return service_data;
}

BleAdvertiser::BleAdvertiser(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const LocalDeviceDataProvider* local_device_data_provider,
    const cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher)
    : BleAdvertiser(adapter,
                    base::MakeUnique<cryptauth::ForegroundEidGenerator>(),
                    remote_beacon_seed_fetcher,
                    local_device_data_provider) {}

BleAdvertiser::~BleAdvertiser() {}

BleAdvertiser::BleAdvertiser(
    scoped_refptr<device::BluetoothAdapter> adapter,
    std::unique_ptr<cryptauth::ForegroundEidGenerator> eid_generator,
    const cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher,
    const LocalDeviceDataProvider* local_device_data_provider)
    : adapter_(adapter),
      eid_generator_(std::move(eid_generator)),
      remote_beacon_seed_fetcher_(remote_beacon_seed_fetcher),
      local_device_data_provider_(local_device_data_provider) {}

bool BleAdvertiser::StartAdvertisingToDevice(
    const cryptauth::RemoteDevice& remote_device) {
  if (device_id_to_advertisement_map_.size() >= kMaxConcurrentAdvertisements) {
    PA_LOG(ERROR) << "Attempted to register a device when the maximum number "
                  << "of devices have already been registered.";
    return false;
  }

  std::string local_device_public_key;
  if (!local_device_data_provider_->GetLocalDeviceData(&local_device_public_key,
                                                       nullptr)) {
    PA_LOG(WARNING) << "Error fetching the local device's public key. Cannot "
                    << "advertise without the public key.";
    return false;
  } else if (local_device_public_key.empty()) {
    PA_LOG(WARNING) << "Local device's public key is empty. Cannot advertise "
                    << "with an invalid key.";
    return false;
  }

  std::vector<cryptauth::BeaconSeed> remote_beacon_seeds;
  if (!remote_beacon_seed_fetcher_->FetchSeedsForDevice(remote_device,
                                                        &remote_beacon_seeds)) {
    PA_LOG(WARNING) << "Error fetching beacon seeds for device with ID "
                    << remote_device.GetTruncatedDeviceIdForLogs() << ". "
                    << "Cannot advertise without seeds.";
    return false;
  } else if (remote_beacon_seeds.empty()) {
    PA_LOG(WARNING) << "No synced seeds exist for device with ID "
                    << remote_device.GetTruncatedDeviceIdForLogs() << ". "
                    << "Cannot advertise without seeds.";
    return false;
  }

  std::unique_ptr<cryptauth::DataWithTimestamp> advertisement =
      eid_generator_->GenerateAdvertisement(local_device_public_key,
                                            remote_beacon_seeds);
  if (!advertisement) {
    PA_LOG(WARNING) << "Error generating advertisement for device with ID "
                    << remote_device.GetTruncatedDeviceIdForLogs() << ". "
                    << "Cannot advertise.";
    return false;
  }

  device_id_to_advertisement_map_[remote_device.GetDeviceId()] =
      base::MakeUnique<IndividualAdvertisement>(
          remote_device.GetDeviceId(), adapter_, std::move(advertisement));
  return true;
}

bool BleAdvertiser::StopAdvertisingToDevice(
    const cryptauth::RemoteDevice& remote_device) {
  return device_id_to_advertisement_map_.erase(remote_device.GetDeviceId()) > 0;
}

}  // namespace tether

}  // namespace chromeos
