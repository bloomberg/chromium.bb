// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SERVICE_DATA_HELPER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SERVICE_DATA_HELPER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/secure_channel/data_with_timestamp.h"
#include "chromeos/services/secure_channel/device_id_pair.h"

namespace chromeos {

namespace secure_channel {

// Provides the ability to generate BLE advertisement service data and, given
// service data that has been received in a BLE discovery session, identify the
// device which sent the advertisement.
class BleServiceDataHelper {
 public:
  virtual ~BleServiceDataHelper();

  // Generates service data to be used in a foreground BLE advertisement from
  // the device with ID |local_device_id| to the device with ID
  // |remote_device_id|. If no service data can be generated, null is returned.
  virtual std::unique_ptr<DataWithTimestamp> GenerateForegroundAdvertisement(
      const DeviceIdPair& device_id_pair) = 0;

  // Remote device paired with a boolean of whether the device was identified
  // via the background advertisement scheme.
  using DeviceWithBackgroundBool =
      std::pair<multidevice::RemoteDeviceRef, bool>;

  // Identifies the device that produced a BLE advertisement with service data
  // |service_data|. If no device can be identified, base::nullopt is returned.
  base::Optional<DeviceWithBackgroundBool> IdentifyRemoteDevice(
      const std::string& service_data,
      const DeviceIdPairSet& device_id_pair_set);

 protected:
  BleServiceDataHelper();

  virtual base::Optional<DeviceWithBackgroundBool> PerformIdentifyRemoteDevice(
      const std::string& service_data,
      const DeviceIdPairSet& device_id_pair_set) = 0;

  DISALLOW_COPY_AND_ASSIGN(BleServiceDataHelper);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_SERVICE_DATA_HELPER_H_
