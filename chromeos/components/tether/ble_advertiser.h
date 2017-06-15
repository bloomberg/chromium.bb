// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISER_H_
#define CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/cryptauth/foreground_eid_generator.h"
#include "components/cryptauth/remote_device.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace cryptauth {
class RemoteBeaconSeedFetcher;
}

namespace chromeos {

namespace tether {

class LocalDeviceDataProvider;

// Advertises to a given device. When StartAdvertisingToDevice() is called, a
// device-specific EID value is computed deterministically and is set as the
// service data of the advertisement. If that device is nearby and scanning,
// the device will have the same service data and will be able to pick up the
// advertisement.
class BleAdvertiser {
 public:
  BleAdvertiser(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const LocalDeviceDataProvider* local_device_data_provider,
      const cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher);
  virtual ~BleAdvertiser();

  virtual bool StartAdvertisingToDevice(
      const cryptauth::RemoteDevice& remote_device);
  virtual bool StopAdvertisingToDevice(
      const cryptauth::RemoteDevice& remote_device);

 private:
  friend class BleAdvertiserTest;

  class IndividualAdvertisement
      : public device::BluetoothAdapter::Observer,
        public device::BluetoothAdvertisement::Observer {
   public:
    IndividualAdvertisement(
        const std::string& device_id,
        scoped_refptr<device::BluetoothAdapter> adapter,
        std::unique_ptr<cryptauth::DataWithTimestamp> advertisement_data);
    ~IndividualAdvertisement() override;

    // device::BluetoothAdapter::Observer
    void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                               bool powered) override;

    // device::BluetoothAdvertisement::Observer
    void AdvertisementReleased(
        device::BluetoothAdvertisement* advertisement) override;

   private:
    friend class BleAdvertiserTest;

    void AdvertiseIfPossible();
    void OnAdvertisementRegisteredCallback(
        scoped_refptr<device::BluetoothAdvertisement> advertisement);
    void OnAdvertisementErrorCallback(
        device::BluetoothAdvertisement::ErrorCode error_code);
    void OnAdvertisementUnregisterFailure(
        device::BluetoothAdvertisement::ErrorCode error_code);

    std::unique_ptr<device::BluetoothAdvertisement::UUIDList>
    CreateServiceUuids() const;

    std::unique_ptr<device::BluetoothAdvertisement::ServiceData>
    CreateServiceData() const;

    std::string device_id_;
    scoped_refptr<device::BluetoothAdapter> adapter_;
    std::unique_ptr<cryptauth::DataWithTimestamp> advertisement_data_;
    bool is_initializing_advertising_;
    scoped_refptr<device::BluetoothAdvertisement> advertisement_;

    base::WeakPtrFactory<IndividualAdvertisement> weak_ptr_factory_;

    DISALLOW_COPY_AND_ASSIGN(IndividualAdvertisement);
  };

  BleAdvertiser(
      scoped_refptr<device::BluetoothAdapter> adapter,
      std::unique_ptr<cryptauth::ForegroundEidGenerator> eid_generator,
      const cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher,
      const LocalDeviceDataProvider* local_device_data_provider);

  scoped_refptr<device::BluetoothAdapter> adapter_;

  std::unique_ptr<cryptauth::ForegroundEidGenerator> eid_generator_;
  // Not owned by this instance and must outlive it.
  const cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher_;
  const LocalDeviceDataProvider* local_device_data_provider_;

  std::map<std::string, std::unique_ptr<IndividualAdvertisement>>
      device_id_to_advertisement_map_;

  DISALLOW_COPY_AND_ASSIGN(BleAdvertiser);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISER_H_
