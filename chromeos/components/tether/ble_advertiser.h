// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISER_H_
#define CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISER_H_

#include <map>

#include "base/macros.h"

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/cryptauth/eid_generator.h"
#include "components/cryptauth/remote_device.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace cryptauth {
class RemoteBeaconSeedFetcher;
}

namespace chromeos {

namespace tether {

class LocalDeviceDataProvider;

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
        public device::BluetoothAdvertisement::Observer,
        public base::RefCounted<IndividualAdvertisement> {
   public:
    IndividualAdvertisement(
        scoped_refptr<device::BluetoothAdapter> adapter,
        std::unique_ptr<cryptauth::EidGenerator::DataWithTimestamp>
            advertisement_data);

    // device::BluetoothAdapter::Observer
    void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                               bool powered) override;

    // device::BluetoothAdvertisement::Observer
    void AdvertisementReleased(
        device::BluetoothAdvertisement* advertisement) override;

   private:
    friend class base::RefCounted<IndividualAdvertisement>;
    friend class BleAdvertiserTest;

    ~IndividualAdvertisement() override;

    void AdvertiseIfPossible();
    void OnAdvertisementRegisteredCallback(
        scoped_refptr<device::BluetoothAdvertisement> advertisement);
    void OnAdvertisementErrorCallback(
        device::BluetoothAdvertisement::ErrorCode error_code);

    std::unique_ptr<device::BluetoothAdvertisement::UUIDList>
    CreateServiceUuids() const;

    std::unique_ptr<device::BluetoothAdvertisement::ServiceData>
    CreateServiceData() const;

    std::string ServiceDataInHex() const;

    scoped_refptr<device::BluetoothAdapter> adapter_;
    bool is_initializing_advertising_;
    std::unique_ptr<cryptauth::EidGenerator::DataWithTimestamp>
        advertisement_data_;
    scoped_refptr<device::BluetoothAdvertisement> advertisement_;

    base::WeakPtrFactory<IndividualAdvertisement> weak_ptr_factory_;

    DISALLOW_COPY_AND_ASSIGN(IndividualAdvertisement);
  };

  BleAdvertiser(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const cryptauth::EidGenerator* eid_generator,
      const cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher,
      const LocalDeviceDataProvider* local_device_data_provider);

  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Not owned by this instance and must outlive it.
  const cryptauth::EidGenerator* eid_generator_;
  const cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher_;
  const LocalDeviceDataProvider* local_device_data_provider_;

  std::map<std::string, scoped_refptr<IndividualAdvertisement>>
      device_id_to_advertisement_map_;

  DISALLOW_COPY_AND_ASSIGN(BleAdvertiser);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISER_H_
