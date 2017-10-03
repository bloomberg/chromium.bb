// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_advertiser.h"

#include "base/bind.h"
#include "chromeos/components/tether/error_tolerant_ble_advertisement_impl.h"
#include "components/cryptauth/local_device_data_provider.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/remote_beacon_seed_fetcher.h"
#include "components/proximity_auth/logging/logging.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace chromeos {

namespace tether {

BleAdvertiser::AdvertisementMetadata::AdvertisementMetadata(
    const std::string& device_id,
    std::unique_ptr<cryptauth::DataWithTimestamp> service_data)
    : device_id(device_id), service_data(std::move(service_data)) {}

BleAdvertiser::AdvertisementMetadata::~AdvertisementMetadata() {}

BleAdvertiser::BleAdvertiser(
    cryptauth::LocalDeviceDataProvider* local_device_data_provider,
    cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher,
    BleSynchronizerBase* ble_synchronizer)
    : remote_beacon_seed_fetcher_(remote_beacon_seed_fetcher),
      local_device_data_provider_(local_device_data_provider),
      ble_synchronizer_(ble_synchronizer),
      eid_generator_(base::MakeUnique<cryptauth::ForegroundEidGenerator>()),
      weak_ptr_factory_(this) {}

BleAdvertiser::~BleAdvertiser() {}

bool BleAdvertiser::StartAdvertisingToDevice(
    const cryptauth::RemoteDevice& remote_device) {
  int index_for_device = -1;
  for (size_t i = 0; i < kMaxConcurrentAdvertisements; ++i) {
    if (!registered_device_metadata_[i]) {
      index_for_device = i;
      break;
    }
  }

  if (index_for_device == -1) {
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
  }

  if (local_device_public_key.empty()) {
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
  }

  if (remote_beacon_seeds.empty()) {
    PA_LOG(WARNING) << "No synced seeds exist for device with ID "
                    << remote_device.GetTruncatedDeviceIdForLogs() << ". "
                    << "Cannot advertise without seeds.";
    return false;
  }

  std::unique_ptr<cryptauth::DataWithTimestamp> service_data =
      eid_generator_->GenerateAdvertisement(local_device_public_key,
                                            remote_beacon_seeds);
  if (!service_data) {
    PA_LOG(WARNING) << "Error generating advertisement for device with ID "
                    << remote_device.GetTruncatedDeviceIdForLogs() << ". "
                    << "Cannot advertise.";
    return false;
  }

  registered_device_metadata_[index_for_device].reset(new AdvertisementMetadata(
      remote_device.GetDeviceId(), std::move(service_data)));
  UpdateAdvertisements();

  return true;
}

bool BleAdvertiser::StopAdvertisingToDevice(
    const cryptauth::RemoteDevice& remote_device) {
  for (auto& metadata : registered_device_metadata_) {
    if (metadata && metadata->device_id == remote_device.GetDeviceId()) {
      metadata.reset();
      UpdateAdvertisements();
      return true;
    }
  }

  return false;
}

bool BleAdvertiser::AreAdvertisementsRegistered() {
  for (const auto& advertisement : advertisements_) {
    if (advertisement)
      return true;
  }

  return false;
}

void BleAdvertiser::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void BleAdvertiser::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void BleAdvertiser::NotifyAllAdvertisementsUnregistered() {
  for (auto& observer : observer_list_)
    observer.OnAllAdvertisementsUnregistered();
}

void BleAdvertiser::SetEidGeneratorForTest(
    std::unique_ptr<cryptauth::ForegroundEidGenerator> test_eid_generator) {
  eid_generator_ = std::move(test_eid_generator);
}

void BleAdvertiser::UpdateAdvertisements() {
  for (size_t i = 0; i < kMaxConcurrentAdvertisements; ++i) {
    std::unique_ptr<AdvertisementMetadata>& metadata =
        registered_device_metadata_[i];
    std::unique_ptr<ErrorTolerantBleAdvertisement>& advertisement =
        advertisements_[i];

    // If there is a registered device but no associated advertisement, create
    // the advertisement.
    if (metadata && !advertisement) {
      std::unique_ptr<cryptauth::DataWithTimestamp> service_data_copy =
          base::MakeUnique<cryptauth::DataWithTimestamp>(
              *metadata->service_data);
      advertisements_[i] =
          ErrorTolerantBleAdvertisementImpl::Factory::NewInstance(
              metadata->device_id, std::move(service_data_copy),
              ble_synchronizer_);
      continue;
    }

    // If there is no registered device but there is an advertisement, stop it
    // if it has not yet been stopped.
    if (!metadata && advertisement && !advertisement->HasBeenStopped()) {
      advertisement->Stop(base::Bind(&BleAdvertiser::OnAdvertisementStopped,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     i /* index */));
    }
  }
}

void BleAdvertiser::OnAdvertisementStopped(size_t index) {
  DCHECK(advertisements_[index] && advertisements_[index]->HasBeenStopped());
  advertisements_[index].reset();

  if (!AreAdvertisementsRegistered())
    NotifyAllAdvertisementsUnregistered();

  UpdateAdvertisements();
}

}  // namespace tether

}  // namespace chromeos
