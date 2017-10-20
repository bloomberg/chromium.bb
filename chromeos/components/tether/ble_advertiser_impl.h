// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISER_IMPL_H_
#define CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISER_IMPL_H_

#include <array>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/tether/ble_advertiser.h"
#include "chromeos/components/tether/ble_constants.h"
#include "components/cryptauth/foreground_eid_generator.h"
#include "components/cryptauth/remote_device.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace cryptauth {
class LocalDeviceDataProvider;
class RemoteBeaconSeedFetcher;
}  // namespace cryptauth

namespace chromeos {

namespace tether {

class BleSynchronizerBase;
class ErrorTolerantBleAdvertisement;

// Concrete BleAdvertiser implementation.
class BleAdvertiserImpl : public BleAdvertiser {
 public:
  class Factory {
   public:
    static std::unique_ptr<BleAdvertiser> NewInstance(
        cryptauth::LocalDeviceDataProvider* local_device_data_provider,
        cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher,
        BleSynchronizerBase* ble_synchronizer);
    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<BleAdvertiser> BuildInstance(
        cryptauth::LocalDeviceDataProvider* local_device_data_provider,
        cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher,
        BleSynchronizerBase* ble_synchronizer);

   private:
    static Factory* factory_instance_;
  };

  BleAdvertiserImpl(
      cryptauth::LocalDeviceDataProvider* local_device_data_provider,
      cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher,
      BleSynchronizerBase* ble_synchronizer);
  ~BleAdvertiserImpl() override;

  // BleAdvertiser:
  bool StartAdvertisingToDevice(
      const cryptauth::RemoteDevice& remote_device) override;
  bool StopAdvertisingToDevice(
      const cryptauth::RemoteDevice& remote_device) override;
  bool AreAdvertisementsRegistered() override;

 private:
  friend class BleAdvertiserImplTest;

  // Data needed to generate an advertisement.
  struct AdvertisementMetadata {
    AdvertisementMetadata(
        const std::string& device_id,
        std::unique_ptr<cryptauth::DataWithTimestamp> service_data);
    ~AdvertisementMetadata();

    std::string device_id;
    std::unique_ptr<cryptauth::DataWithTimestamp> service_data;
  };

  void SetTestDoubles(
      std::unique_ptr<cryptauth::ForegroundEidGenerator> test_eid_generator,
      scoped_refptr<base::TaskRunner> test_task_runner);
  void UpdateAdvertisements();
  void OnAdvertisementStopped(size_t index);

  cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher_;
  cryptauth::LocalDeviceDataProvider* local_device_data_provider_;
  BleSynchronizerBase* ble_synchronizer_;

  std::unique_ptr<cryptauth::ForegroundEidGenerator> eid_generator_;
  scoped_refptr<base::TaskRunner> task_runner_;

  // |registered_device_ids_| holds the device IDs that are currently
  // registered and is always up-to-date. |advertisements_| contains the active
  // advertisements, which may not correspond exactly to
  // |registered_device_ids_| in the case that a previous advertisement failed
  // to unregister.
  std::array<std::unique_ptr<AdvertisementMetadata>,
             kMaxConcurrentAdvertisements>
      registered_device_metadata_;
  std::array<std::unique_ptr<ErrorTolerantBleAdvertisement>,
             kMaxConcurrentAdvertisements>
      advertisements_;

  base::WeakPtrFactory<BleAdvertiserImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BleAdvertiserImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISER_IMPL_H_
