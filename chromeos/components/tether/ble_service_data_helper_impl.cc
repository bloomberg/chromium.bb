// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_service_data_helper_impl.h"

#include <algorithm>
#include <iterator>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/chromeos_switches.h"
#include "components/cryptauth/background_eid_generator.h"
#include "components/cryptauth/ble/ble_advertisement_generator.h"
#include "components/cryptauth/foreground_eid_generator.h"
#include "components/cryptauth/local_device_data_provider.h"
#include "components/cryptauth/remote_beacon_seed_fetcher.h"

namespace chromeos {

namespace tether {

namespace {

// Valid advertisement service data must be at least 2 bytes.
// As of March 2018, valid background advertisement service data is exactly 2
// bytes, which identify the advertising device to the scanning device.
// Valid foreground advertisement service data must include at least 4 bytes:
// 2 bytes associated with the scanning device (used as a scan filter) and 2
// bytes which identify the advertising device to the scanning device.
const size_t kMinNumBytesInServiceData = 2;
const size_t kMaxNumBytesInBackgroundServiceData = 3;
const size_t kMinNumBytesInForegroundServiceData = 4;

}  // namespace

// static
BleServiceDataHelperImpl::Factory*
    BleServiceDataHelperImpl::Factory::test_factory_ = nullptr;

// static
BleServiceDataHelperImpl::Factory* BleServiceDataHelperImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void BleServiceDataHelperImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

BleServiceDataHelperImpl::Factory::~Factory() = default;

std::unique_ptr<secure_channel::BleServiceDataHelper>
BleServiceDataHelperImpl::Factory::BuildInstance(
    TetherHostFetcher* tether_host_fetcher,
    cryptauth::LocalDeviceDataProvider* local_device_data_provider,
    cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher) {
  return base::WrapUnique(new BleServiceDataHelperImpl(
      tether_host_fetcher, local_device_data_provider,
      remote_beacon_seed_fetcher));
}

BleServiceDataHelperImpl::BleServiceDataHelperImpl(
    TetherHostFetcher* tether_host_fetcher,
    cryptauth::LocalDeviceDataProvider* local_device_data_provider,
    cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher)
    : tether_host_fetcher_(tether_host_fetcher),
      local_device_data_provider_(local_device_data_provider),
      remote_beacon_seed_fetcher_(remote_beacon_seed_fetcher),
      background_eid_generator_(
          std::make_unique<cryptauth::BackgroundEidGenerator>()),
      foreground_eid_generator_(
          std::make_unique<cryptauth::ForegroundEidGenerator>()),
      weak_ptr_factory_(this) {
  tether_host_fetcher_->AddObserver(this);
  OnTetherHostsUpdated();
}

BleServiceDataHelperImpl::~BleServiceDataHelperImpl() {
  tether_host_fetcher_->RemoveObserver(this);
}

std::unique_ptr<cryptauth::DataWithTimestamp>
BleServiceDataHelperImpl::GenerateForegroundAdvertisement(
    const secure_channel::DeviceIdPair& device_id_pair) {
  return cryptauth::BleAdvertisementGenerator::GenerateBleAdvertisement(
      device_id_pair.remote_device_id(), local_device_data_provider_,
      remote_beacon_seed_fetcher_);
}

base::Optional<secure_channel::BleServiceDataHelper::DeviceWithBackgroundBool>
BleServiceDataHelperImpl::PerformIdentifyRemoteDevice(
    const std::string& service_data,
    const secure_channel::DeviceIdPairSet& device_id_pair_set) {
  std::string device_id;
  bool is_background_advertisement = false;

  // First try, identifying |service_data| as a foreground advertisement.
  if (service_data.size() >= kMinNumBytesInForegroundServiceData) {
    std::vector<cryptauth::BeaconSeed> beacon_seeds;
    if (local_device_data_provider_->GetLocalDeviceData(nullptr,
                                                        &beacon_seeds)) {
      device_id =
          foreground_eid_generator_->IdentifyRemoteDeviceByAdvertisement(
              service_data, registered_remote_device_ids_, beacon_seeds);
    }
  }

  // If the device has not yet been identified, try identifying |service_data|
  // as a background advertisement.
  if (chromeos::switches::IsInstantTetheringBackgroundAdvertisingSupported() &&
      device_id.empty() && service_data.size() >= kMinNumBytesInServiceData &&
      service_data.size() <= kMaxNumBytesInBackgroundServiceData) {
    device_id = background_eid_generator_->IdentifyRemoteDeviceByAdvertisement(
        remote_beacon_seed_fetcher_, service_data,
        registered_remote_device_ids_);
    is_background_advertisement = true;
  }

  // If the service data does not correspond to an advertisement from a device
  // on this account, ignore it.
  if (device_id.empty())
    return base::nullopt;

  for (const auto& remote_device_ref : tether_hosts_from_last_fetch_) {
    if (remote_device_ref.GetDeviceId() == device_id) {
      return secure_channel::BleServiceDataHelper::DeviceWithBackgroundBool(
          remote_device_ref, is_background_advertisement);
    }
  }

  NOTREACHED();
  return base::nullopt;
}

void BleServiceDataHelperImpl::OnTetherHostsUpdated() {
  tether_host_fetcher_->FetchAllTetherHosts(
      base::Bind(&BleServiceDataHelperImpl::OnTetherHostsFetched,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BleServiceDataHelperImpl::OnTetherHostsFetched(
    const cryptauth::RemoteDeviceRefList& tether_hosts) {
  tether_hosts_from_last_fetch_ = tether_hosts;

  registered_remote_device_ids_.clear();
  std::transform(tether_hosts_from_last_fetch_.begin(),
                 tether_hosts_from_last_fetch_.end(),
                 std::back_inserter(registered_remote_device_ids_),
                 [](const auto& host) { return host.GetDeviceId(); });
}

}  // namespace tether

}  // namespace chromeos
