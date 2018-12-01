// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/public/mojom/device_sync_mojom_traits.h"

#include "base/logging.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
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
}

std::string StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
                         chromeos::multidevice::RemoteDevice>::
    device_id(const chromeos::multidevice::RemoteDevice& remote_device) {
  return remote_device.GetDeviceId();
}

const std::string&
StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
             chromeos::multidevice::RemoteDevice>::
    user_id(const chromeos::multidevice::RemoteDevice& remote_device) {
  return remote_device.user_id;
}

const std::string&
StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
             chromeos::multidevice::RemoteDevice>::
    device_name(const chromeos::multidevice::RemoteDevice& remote_device) {
  return remote_device.name;
}

const std::string&
StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
             chromeos::multidevice::RemoteDevice>::
    persistent_symmetric_key(
        const chromeos::multidevice::RemoteDevice& remote_device) {
  return remote_device.persistent_symmetric_key;
}

base::Time StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
                        chromeos::multidevice::RemoteDevice>::
    last_update_time(const chromeos::multidevice::RemoteDevice& remote_device) {
  return base::Time::FromJavaTime(remote_device.last_update_time_millis);
}

const std::map<cryptauth::SoftwareFeature,
               chromeos::multidevice::SoftwareFeatureState>&
StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
             chromeos::multidevice::RemoteDevice>::
    software_features(
        const chromeos::multidevice::RemoteDevice& remote_device) {
  return remote_device.software_features;
}

const std::vector<cryptauth::BeaconSeed>&
StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
             chromeos::multidevice::RemoteDevice>::
    beacon_seeds(const chromeos::multidevice::RemoteDevice& remote_device) {
  return remote_device.beacon_seeds;
}

bool StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
                  chromeos::multidevice::RemoteDevice>::
    Read(chromeos::device_sync::mojom::RemoteDeviceDataView in,
         chromeos::multidevice::RemoteDevice* out) {
  std::string device_id;
  base::Time last_update_time;

  if (!in.ReadUserId(&out->user_id) || !in.ReadDeviceName(&out->name) ||
      !in.ReadDeviceId(&device_id) ||
      !in.ReadPersistentSymmetricKey(&out->persistent_symmetric_key) ||
      !in.ReadLastUpdateTime(&last_update_time) ||
      !in.ReadSoftwareFeatures(&out->software_features) ||
      !in.ReadBeaconSeeds(&out->beacon_seeds)) {
    return false;
  }

  out->public_key =
      chromeos::multidevice::RemoteDeviceRef::DerivePublicKey(device_id);
  out->last_update_time_millis = last_update_time.ToJavaTime();

  return true;
}

chromeos::device_sync::mojom::SoftwareFeature EnumTraits<
    chromeos::device_sync::mojom::SoftwareFeature,
    cryptauth::SoftwareFeature>::ToMojom(cryptauth::SoftwareFeature input) {
  switch (input) {
    case cryptauth::SoftwareFeature::UNKNOWN_FEATURE:
      return chromeos::device_sync::mojom::SoftwareFeature::UNKNOWN_FEATURE;
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST:
      return chromeos::device_sync::mojom::SoftwareFeature::
          BETTER_TOGETHER_HOST;
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT:
      return chromeos::device_sync::mojom::SoftwareFeature::
          BETTER_TOGETHER_CLIENT;
    case cryptauth::SoftwareFeature::EASY_UNLOCK_HOST:
      return chromeos::device_sync::mojom::SoftwareFeature::EASY_UNLOCK_HOST;
    case cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT:
      return chromeos::device_sync::mojom::SoftwareFeature::EASY_UNLOCK_CLIENT;
    case cryptauth::SoftwareFeature::MAGIC_TETHER_HOST:
      return chromeos::device_sync::mojom::SoftwareFeature::MAGIC_TETHER_HOST;
    case cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT:
      return chromeos::device_sync::mojom::SoftwareFeature::MAGIC_TETHER_CLIENT;
    case cryptauth::SoftwareFeature::SMS_CONNECT_HOST:
      return chromeos::device_sync::mojom::SoftwareFeature::SMS_CONNECT_HOST;
    case cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT:
      return chromeos::device_sync::mojom::SoftwareFeature::SMS_CONNECT_CLIENT;
  }

  NOTREACHED();
  return chromeos::device_sync::mojom::SoftwareFeature::UNKNOWN_FEATURE;
}

bool EnumTraits<chromeos::device_sync::mojom::SoftwareFeature,
                cryptauth::SoftwareFeature>::
    FromMojom(chromeos::device_sync::mojom::SoftwareFeature input,
              cryptauth::SoftwareFeature* out) {
  switch (input) {
    case chromeos::device_sync::mojom::SoftwareFeature::UNKNOWN_FEATURE:
      *out = cryptauth::SoftwareFeature::UNKNOWN_FEATURE;
      return true;
    case chromeos::device_sync::mojom::SoftwareFeature::BETTER_TOGETHER_HOST:
      *out = cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST;
      return true;
    case chromeos::device_sync::mojom::SoftwareFeature::BETTER_TOGETHER_CLIENT:
      *out = cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT;
      return true;
    case chromeos::device_sync::mojom::SoftwareFeature::EASY_UNLOCK_HOST:
      *out = cryptauth::SoftwareFeature::EASY_UNLOCK_HOST;
      return true;
    case chromeos::device_sync::mojom::SoftwareFeature::EASY_UNLOCK_CLIENT:
      *out = cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT;
      return true;
    case chromeos::device_sync::mojom::SoftwareFeature::MAGIC_TETHER_HOST:
      *out = cryptauth::SoftwareFeature::MAGIC_TETHER_HOST;
      return true;
    case chromeos::device_sync::mojom::SoftwareFeature::MAGIC_TETHER_CLIENT:
      *out = cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT;
      return true;
    case chromeos::device_sync::mojom::SoftwareFeature::SMS_CONNECT_HOST:
      *out = cryptauth::SoftwareFeature::SMS_CONNECT_HOST;
      return true;
    case chromeos::device_sync::mojom::SoftwareFeature::SMS_CONNECT_CLIENT:
      *out = cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT;
      return true;
  }

  NOTREACHED();
  return false;
}

chromeos::device_sync::mojom::SoftwareFeatureState
EnumTraits<chromeos::device_sync::mojom::SoftwareFeatureState,
           chromeos::multidevice::SoftwareFeatureState>::
    ToMojom(chromeos::multidevice::SoftwareFeatureState input) {
  switch (input) {
    case chromeos::multidevice::SoftwareFeatureState::kNotSupported:
      return chromeos::device_sync::mojom::SoftwareFeatureState::kNotSupported;
    case chromeos::multidevice::SoftwareFeatureState::kSupported:
      return chromeos::device_sync::mojom::SoftwareFeatureState::kSupported;
    case chromeos::multidevice::SoftwareFeatureState::kEnabled:
      return chromeos::device_sync::mojom::SoftwareFeatureState::kEnabled;
  }

  NOTREACHED();
  return chromeos::device_sync::mojom::SoftwareFeatureState::kNotSupported;
}

bool EnumTraits<chromeos::device_sync::mojom::SoftwareFeatureState,
                chromeos::multidevice::SoftwareFeatureState>::
    FromMojom(chromeos::device_sync::mojom::SoftwareFeatureState input,
              chromeos::multidevice::SoftwareFeatureState* out) {
  switch (input) {
    case chromeos::device_sync::mojom::SoftwareFeatureState::kNotSupported:
      *out = chromeos::multidevice::SoftwareFeatureState::kNotSupported;
      return true;
    case chromeos::device_sync::mojom::SoftwareFeatureState::kSupported:
      *out = chromeos::multidevice::SoftwareFeatureState::kSupported;
      return true;
    case chromeos::device_sync::mojom::SoftwareFeatureState::kEnabled:
      *out = chromeos::multidevice::SoftwareFeatureState::kEnabled;
      return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace mojo
