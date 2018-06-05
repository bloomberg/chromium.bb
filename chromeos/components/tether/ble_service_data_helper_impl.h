// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_BLE_SERVICE_DATA_HELPER_IMPL_H_
#define CHROMEOS_COMPONENTS_TETHER_BLE_SERVICE_DATA_HELPER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/services/secure_channel/ble_service_data_helper.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "components/cryptauth/data_with_timestamp.h"
#include "components/cryptauth/remote_device_ref.h"

namespace cryptauth {
class BackgroundEidGenerator;
class ForegroundEidGenerator;
class LocalDeviceDataProvider;
class RemoteBeaconSeedFetcher;
}  // namespace cryptauth

namespace chromeos {

namespace tether {

// chromeos::tether BleServiceDataHelper implementation.
class BleServiceDataHelperImpl : public secure_channel::BleServiceDataHelper,
                                 public TetherHostFetcher::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<secure_channel::BleServiceDataHelper> BuildInstance(
        TetherHostFetcher* tether_host_fetcher,
        cryptauth::LocalDeviceDataProvider* local_device_data_provider,
        cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher);

   private:
    static Factory* test_factory_;
  };

  ~BleServiceDataHelperImpl() override;

 private:
  BleServiceDataHelperImpl(
      TetherHostFetcher* tether_host_fetcher,
      cryptauth::LocalDeviceDataProvider* local_device_data_provider,
      cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher);

  // secure_channel::BleServiceDataHelper:
  std::unique_ptr<cryptauth::DataWithTimestamp> GenerateForegroundAdvertisement(
      const secure_channel::DeviceIdPair& device_id_pair) override;
  base::Optional<DeviceWithBackgroundBool> PerformIdentifyRemoteDevice(
      const std::string& service_data,
      const secure_channel::DeviceIdPairSet& device_id_pair_set) override;

  // TetherHostFetcher::Observer:
  void OnTetherHostsUpdated() override;

  void OnTetherHostsFetched(const cryptauth::RemoteDeviceRefList& tether_hosts);

  TetherHostFetcher* tether_host_fetcher_;
  cryptauth::LocalDeviceDataProvider* local_device_data_provider_;
  cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher_;

  std::unique_ptr<cryptauth::BackgroundEidGenerator> background_eid_generator_;
  std::unique_ptr<cryptauth::ForegroundEidGenerator> foreground_eid_generator_;

  cryptauth::RemoteDeviceRefList tether_hosts_from_last_fetch_;
  std::vector<std::string> registered_remote_device_ids_;

  base::WeakPtrFactory<BleServiceDataHelperImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BleServiceDataHelperImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_BLE_SERVICE_DATA_HELPER_IMPL_H_
