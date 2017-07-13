// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_advertiser.h"

#include "base/bind.h"
#include "chromeos/components/tether/ble_constants.h"
#include "components/cryptauth/local_device_data_provider.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/remote_beacon_seed_fetcher.h"
#include "components/proximity_auth/logging/logging.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace chromeos {

namespace tether {

namespace {

uint8_t kInvertedConnectionFlag = 0x01;

// Handles the unregistration of a BluetoothAdvertisement in the case that its
// intended owner (an IndividualAdvertisement) was destroyed before successful
// registration. See
// BleAdvertiser::IndividualAdvertisement::OnAdvertisementRegistered().
void OnAdvertisementUnregisteredAfterOwnerDestruction(
    const std::string& associated_device_id) {
  PA_LOG(ERROR) << "Unregistered advertisement for device ID: "
                << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(
                       associated_device_id);
}

// Handles the failed unregistration of a BluetoothAdvertisement in the case
// that its intended owner (an IndividualAdvertisement) was destroyed before
// successful registration. See
// BleAdvertiser::IndividualAdvertisement::OnAdvertisementRegistered().
//
// It is not expected that this function ever be called; that would indicate an
// error in Bluetooth.
void OnAdvertisementUnregisteredAfterOwnerDestructionFailure(
    const std::string& associated_device_id,
    device::BluetoothAdvertisement::ErrorCode error_code) {
  PA_LOG(ERROR) << "Error unregistering advertisement. "
                << "Device ID: \""
                << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(
                       associated_device_id)
                << "\", Error code: " << error_code;
}

}  // namespace

// TODO (hansberry): Remove this workaround once crbug.com/741050 has been
// resolved.
// static
void BleAdvertiser::IndividualAdvertisement::OnAdvertisementRegistered(
    base::WeakPtr<BleAdvertiser::IndividualAdvertisement>
        individual_advertisement,
    const std::string& associated_device_id,
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  // It's possible that the IndividualAdvertisement that registered this
  // BluetoothAdvertisement has been destroyed before being able to own the
  // BluetoothAdvertisement. If the IndividualAdvertisement still exists, simply
  // give it the BluetoothAdvertisement it registered. If not, unregister the
  // BluetoothAdvertisement, because otherwise it will remain registered
  // forever.
  if (individual_advertisement.get()) {
    individual_advertisement->OnAdvertisementRegisteredCallback(advertisement);
  } else {
    PA_LOG(WARNING) << "BluetoothAdvertisement registered, but the "
                    << "IndividualAdvertisement which registered it no longer "
                    << "exists. Unregistering the BluetoothAdvertisement.";
    advertisement->Unregister(
        base::Bind(&OnAdvertisementUnregisteredAfterOwnerDestruction,
                   associated_device_id),
        base::Bind(&OnAdvertisementUnregisteredAfterOwnerDestructionFailure,
                   associated_device_id));
  }
}

BleAdvertiser::IndividualAdvertisement::IndividualAdvertisement(
    const std::string& device_id,
    scoped_refptr<device::BluetoothAdapter> adapter,
    std::unique_ptr<cryptauth::DataWithTimestamp> advertisement_data,
    const base::Closure& on_unregister_advertisement_success_callback,
    const base::Callback<void(device::BluetoothAdvertisement::ErrorCode)>&
        on_unregister_advertisement_error_callback,
    std::unordered_set<std::string>* active_advertisement_device_ids_set)
    : device_id_(device_id),
      adapter_(adapter),
      advertisement_data_(std::move(advertisement_data)),
      is_initializing_advertising_(false),
      advertisement_(nullptr),
      on_unregister_advertisement_success_callback_(
          on_unregister_advertisement_success_callback),
      on_unregister_advertisement_error_callback_(
          on_unregister_advertisement_error_callback),
      active_advertisement_device_ids_set_(active_advertisement_device_ids_set),
      weak_ptr_factory_(this) {
  adapter_->AddObserver(this);
  AdvertiseIfPossible();
}

BleAdvertiser::IndividualAdvertisement::~IndividualAdvertisement() {
  if (advertisement_) {
    advertisement_->Unregister(on_unregister_advertisement_success_callback_,
                               on_unregister_advertisement_error_callback_);
  }

  adapter_->RemoveObserver(this);
}

void BleAdvertiser::IndividualAdvertisement::
    OnPreviousAdvertisementUnregistered() {
  DCHECK(active_advertisement_device_ids_set_->find(device_id_) ==
         active_advertisement_device_ids_set_->end());
  AdvertiseIfPossible();
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
  advertisement_->RemoveObserver(this);
  advertisement_ = nullptr;
  active_advertisement_device_ids_set_->erase(device_id_);

  AdvertiseIfPossible();
}

void BleAdvertiser::IndividualAdvertisement::AdvertiseIfPossible() {
  if (!adapter_->IsPowered() || is_initializing_advertising_ ||
      advertisement_ ||
      active_advertisement_device_ids_set_->find(device_id_) !=
          active_advertisement_device_ids_set_->end()) {
    // It is not possible to advertise if the adapter is not powered. Likewise,
    // we should not try to advertise if there is an advertisement already in
    // progress.
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
      base::Bind(&OnAdvertisementRegistered, weak_ptr_factory_.GetWeakPtr(),
                 device_id_),
      base::Bind(&IndividualAdvertisement::OnAdvertisementErrorCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BleAdvertiser::IndividualAdvertisement::OnAdvertisementRegisteredCallback(
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  is_initializing_advertising_ = false;

  advertisement_ = advertisement;
  advertisement_->AddObserver(this);
  active_advertisement_device_ids_set_->insert(device_id_);

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
    const cryptauth::LocalDeviceDataProvider* local_device_data_provider,
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
    const cryptauth::LocalDeviceDataProvider* local_device_data_provider)
    : adapter_(adapter),
      eid_generator_(std::move(eid_generator)),
      remote_beacon_seed_fetcher_(remote_beacon_seed_fetcher),
      local_device_data_provider_(local_device_data_provider),
      weak_ptr_factory_(this) {}

bool BleAdvertiser::StartAdvertisingToDevice(
    const cryptauth::RemoteDevice& remote_device) {
  if (device_id_to_individual_advertisement_map_.size() >=
      kMaxConcurrentAdvertisements) {
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

  std::string device_id = remote_device.GetDeviceId();
  device_id_to_individual_advertisement_map_[device_id] =
      base::MakeUnique<IndividualAdvertisement>(
          remote_device.GetDeviceId(), adapter_, std::move(advertisement),
          base::Bind(&BleAdvertiser::OnUnregisterAdvertisementSuccess,
                     weak_ptr_factory_.GetWeakPtr(), device_id),
          base::Bind(&BleAdvertiser::OnUnregisterAdvertisementError,
                     weak_ptr_factory_.GetWeakPtr(), device_id),
          &active_advertisement_device_ids_set_);
  return true;
}

bool BleAdvertiser::StopAdvertisingToDevice(
    const cryptauth::RemoteDevice& remote_device) {
  return device_id_to_individual_advertisement_map_.erase(
             remote_device.GetDeviceId()) > 0;
}

void BleAdvertiser::OnUnregisterAdvertisementSuccess(
    const std::string& associated_device_id) {
  RemoveAdvertisingDeviceIdAndRetry(associated_device_id);
}

void BleAdvertiser::OnUnregisterAdvertisementError(
    const std::string& associated_device_id,
    device::BluetoothAdvertisement::ErrorCode error_code) {
  PA_LOG(ERROR) << "Error unregistering advertisement. "
                << "Device ID: \""
                << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(
                       associated_device_id)
                << "\", Error code: " << error_code;

  // Even though there was an error unregistering the advertisement, remove it
  // from the set anyway so that it is possible to try registering the
  // advertisement again. Note that this situation is not expected to occur
  // since unregistering an active advertisement should always succeed.
  RemoveAdvertisingDeviceIdAndRetry(associated_device_id);
}

void BleAdvertiser::RemoveAdvertisingDeviceIdAndRetry(
    const std::string& device_id) {
  active_advertisement_device_ids_set_.erase(device_id);

  auto it = device_id_to_individual_advertisement_map_.find(device_id);
  if (it != device_id_to_individual_advertisement_map_.end())
    it->second->OnPreviousAdvertisementUnregistered();
}

}  // namespace tether

}  // namespace chromeos
