// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/public/mojom/device_sync_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

const std::string& StructTraits<
    chromeos::device_sync::mojom::BeaconSeedDataView,
    cryptauth::BeaconSeed>::data(const cryptauth::BeaconSeed& beacon_seed) {
  return beacon_seed.data();
}

base::Time
StructTraits<chromeos::device_sync::mojom::BeaconSeedDataView,
             cryptauth::BeaconSeed>::start_time(const cryptauth::BeaconSeed&
                                                    beacon_seed) {
  return base::Time::FromJavaTime(beacon_seed.start_time_millis());
}

base::Time StructTraits<
    chromeos::device_sync::mojom::BeaconSeedDataView,
    cryptauth::BeaconSeed>::end_time(const cryptauth::BeaconSeed& beacon_seed) {
  return base::Time::FromJavaTime(beacon_seed.end_time_millis());
}

bool StructTraits<chromeos::device_sync::mojom::BeaconSeedDataView,
                  cryptauth::BeaconSeed>::
    Read(chromeos::device_sync::mojom::BeaconSeedDataView in,
         cryptauth::BeaconSeed* out) {
  std::string beacon_seed_data;
  base::Time start_time;
  base::Time end_time;

  if (!in.ReadData(&beacon_seed_data) || !in.ReadStartTime(&start_time) ||
      !in.ReadEndTime(&end_time)) {
    return false;
  }

  out->set_data(beacon_seed_data);
  out->set_start_time_millis(start_time.ToJavaTime());
  out->set_end_time_millis(end_time.ToJavaTime());

  return true;
};

const std::string&
StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
             cryptauth::RemoteDevice>::public_key(const cryptauth::RemoteDevice&
                                                      remote_device) {
  return remote_device.public_key;
}

const std::string&
StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
             cryptauth::RemoteDevice>::user_id(const cryptauth::RemoteDevice&
                                                   remote_device) {
  return remote_device.user_id;
}

const std::string& StructTraits<
    chromeos::device_sync::mojom::RemoteDeviceDataView,
    cryptauth::RemoteDevice>::device_name(const cryptauth::RemoteDevice&
                                              remote_device) {
  return remote_device.name;
}

const std::string&
StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
             cryptauth::RemoteDevice>::
    persistent_symmetric_key(const cryptauth::RemoteDevice& remote_device) {
  return remote_device.persistent_symmetric_key;
}

bool StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
                  cryptauth::RemoteDevice>::
    unlock_key(const cryptauth::RemoteDevice& remote_device) {
  return remote_device.unlock_key;
}

bool StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
                  cryptauth::RemoteDevice>::
    supports_mobile_hotspot(const cryptauth::RemoteDevice& remote_device) {
  return remote_device.supports_mobile_hotspot;
}

base::Time StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
                        cryptauth::RemoteDevice>::
    last_update_time(const cryptauth::RemoteDevice& remote_device) {
  return base::Time::FromJavaTime(remote_device.last_update_time_millis);
}

const std::vector<cryptauth::BeaconSeed>& StructTraits<
    chromeos::device_sync::mojom::RemoteDeviceDataView,
    cryptauth::RemoteDevice>::beacon_seeds(const cryptauth::RemoteDevice&
                                               remote_device) {
  return remote_device.beacon_seeds;
}

bool StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
                  cryptauth::RemoteDevice>::
    Read(chromeos::device_sync::mojom::RemoteDeviceDataView in,
         cryptauth::RemoteDevice* out) {
  std::string user_id;
  std::string device_name;
  std::string public_key;
  std::string persistent_symmetric_key;
  base::Time last_update_time;
  std::vector<cryptauth::BeaconSeed> beacon_seeds_out;

  if (!in.ReadUserId(&user_id) || !in.ReadDeviceName(&device_name) ||
      !in.ReadPublicKey(&public_key) ||
      !in.ReadPersistentSymmetricKey(&persistent_symmetric_key) ||
      !in.ReadLastUpdateTime(&last_update_time) ||
      !in.ReadBeaconSeeds(&beacon_seeds_out)) {
    return false;
  }

  out->user_id = user_id;
  out->name = device_name;
  out->public_key = public_key;
  out->persistent_symmetric_key = persistent_symmetric_key;
  out->unlock_key = in.unlock_key();
  out->supports_mobile_hotspot = in.supports_mobile_hotspot();
  out->last_update_time_millis = last_update_time.ToJavaTime();

  out->LoadBeaconSeeds(beacon_seeds_out);

  return true;
}

}  // namespace mojo
