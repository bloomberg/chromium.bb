// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_MOJOM_DEVICE_SYNC_MOJOM_TRAITS_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_MOJOM_DEVICE_SYNC_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "chromeos/components/multidevice/remote_device.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
class StructTraits<chromeos::device_sync::mojom::BeaconSeedDataView,
                   cryptauth::BeaconSeed> {
 public:
  static const std::string& data(const cryptauth::BeaconSeed& beacon_seed);
  static base::Time start_time(const cryptauth::BeaconSeed& beacon_seed);
  static base::Time end_time(const cryptauth::BeaconSeed& beacon_seed);

  static bool Read(chromeos::device_sync::mojom::BeaconSeedDataView in,
                   cryptauth::BeaconSeed* out);
};

template <>
class StructTraits<chromeos::device_sync::mojom::RemoteDeviceDataView,
                   chromeos::multidevice::RemoteDevice> {
 public:
  static std::string device_id(
      const chromeos::multidevice::RemoteDevice& remote_device);
  static const std::string& user_id(
      const chromeos::multidevice::RemoteDevice& remote_device);
  static const std::string& device_name(
      const chromeos::multidevice::RemoteDevice& remote_device);
  static const std::string& persistent_symmetric_key(
      const chromeos::multidevice::RemoteDevice& remote_device);
  static base::Time last_update_time(
      const chromeos::multidevice::RemoteDevice& remote_device);
  static const std::map<cryptauth::SoftwareFeature,
                        chromeos::multidevice::SoftwareFeatureState>&
  software_features(const chromeos::multidevice::RemoteDevice& remote_device);
  static const std::vector<cryptauth::BeaconSeed>& beacon_seeds(
      const chromeos::multidevice::RemoteDevice& remote_device);

  static bool Read(chromeos::device_sync::mojom::RemoteDeviceDataView in,
                   chromeos::multidevice::RemoteDevice* out);
};

template <>
class EnumTraits<chromeos::device_sync::mojom::SoftwareFeature,
                 cryptauth::SoftwareFeature> {
 public:
  static chromeos::device_sync::mojom::SoftwareFeature ToMojom(
      cryptauth::SoftwareFeature input);
  static bool FromMojom(chromeos::device_sync::mojom::SoftwareFeature input,
                        cryptauth::SoftwareFeature* out);
};

template <>
class EnumTraits<chromeos::device_sync::mojom::SoftwareFeatureState,
                 chromeos::multidevice::SoftwareFeatureState> {
 public:
  static chromeos::device_sync::mojom::SoftwareFeatureState ToMojom(
      chromeos::multidevice::SoftwareFeatureState input);
  static bool FromMojom(
      chromeos::device_sync::mojom::SoftwareFeatureState input,
      chromeos::multidevice::SoftwareFeatureState* out);
};

}  // namespace mojo

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_MOJOM_DEVICE_SYNC_MOJOM_TRAITS_H_
