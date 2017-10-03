// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISER_H_
#define CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISER_H_

#include <array>
#include <map>
#include <unordered_set>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/components/tether/ble_constants.h"
#include "components/cryptauth/foreground_eid_generator.h"
#include "components/cryptauth/remote_device.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace cryptauth {
class LocalDeviceDataProvider;
class RemoteBeaconSeedFetcher;
}

namespace chromeos {

namespace tether {

class BleSynchronizerBase;
class ErrorTolerantBleAdvertisement;

// Advertises to a given device. When StartAdvertisingToDevice() is called, a
// device-specific EID value is computed deterministically and is set as the
// service data of the advertisement. If that device is nearby and scanning,
// the device will have the same service data and will be able to pick up the
// advertisement.
class BleAdvertiser {
 public:
  class Observer {
   public:
    virtual void OnAllAdvertisementsUnregistered() = 0;

   protected:
    virtual ~Observer() {}
  };

  BleAdvertiser(cryptauth::LocalDeviceDataProvider* local_device_data_provider,
                cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher,
                BleSynchronizerBase* ble_synchronizer);
  virtual ~BleAdvertiser();

  virtual bool StartAdvertisingToDevice(
      const cryptauth::RemoteDevice& remote_device);
  virtual bool StopAdvertisingToDevice(
      const cryptauth::RemoteDevice& remote_device);

  bool AreAdvertisementsRegistered();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyAllAdvertisementsUnregistered();

 private:
  friend class BleAdvertiserTest;

  // Data needed to generate an advertisement.
  struct AdvertisementMetadata {
    AdvertisementMetadata(
        const std::string& device_id,
        std::unique_ptr<cryptauth::DataWithTimestamp> service_data);
    ~AdvertisementMetadata();

    std::string device_id;
    std::unique_ptr<cryptauth::DataWithTimestamp> service_data;
  };

  void SetEidGeneratorForTest(
      std::unique_ptr<cryptauth::ForegroundEidGenerator> test_eid_generator);
  void UpdateAdvertisements();
  void OnAdvertisementStopped(size_t index);

  cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher_;
  cryptauth::LocalDeviceDataProvider* local_device_data_provider_;
  BleSynchronizerBase* ble_synchronizer_;

  std::unique_ptr<cryptauth::ForegroundEidGenerator> eid_generator_;

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

  base::ObserverList<Observer> observer_list_;

  base::WeakPtrFactory<BleAdvertiser> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BleAdvertiser);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISER_H_
