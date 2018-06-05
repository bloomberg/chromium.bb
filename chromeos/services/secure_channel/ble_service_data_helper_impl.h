// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SERVICE_DATA_HELPER_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SERVICE_DATA_HELPER_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/secure_channel/ble_service_data_helper.h"
#include "components/cryptauth/data_with_timestamp.h"
#include "components/cryptauth/remote_device_ref.h"

namespace cryptauth {
class RemoteDeviceCache;
}  // namespace cryptauth

namespace chromeos {

namespace secure_channel {

// Concrete BleServiceDataHelper implementation.
class BleServiceDataHelperImpl : public BleServiceDataHelper {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<BleServiceDataHelper> BuildInstance(
        cryptauth::RemoteDeviceCache* remote_device_cache);

   private:
    static Factory* test_factory_;
  };

  ~BleServiceDataHelperImpl() override;

 private:
  BleServiceDataHelperImpl(cryptauth::RemoteDeviceCache* remote_device_cache);

  // BleServiceDataHelper:
  std::unique_ptr<cryptauth::DataWithTimestamp> GenerateForegroundAdvertisement(
      const DeviceIdPair& device_id_pair) override;
  base::Optional<DeviceWithBackgroundBool> PerformIdentifyRemoteDevice(
      const std::string& service_data,
      const DeviceIdPairSet& device_id_pair_set) override;

  cryptauth::RemoteDeviceCache* remote_device_cache_;

  DISALLOW_COPY_AND_ASSIGN(BleServiceDataHelperImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SERVICE_DATA_HELPER_IMPL_H_
