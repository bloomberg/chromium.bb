// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_MOJOM_DEVICE_SYNC_MOJOM_TRAITS_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_MOJOM_DEVICE_SYNC_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "chromeos/components/multidevice/beacon_seed.h"
#include "chromeos/components/multidevice/remote_device.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
class StructTraits<chromeos::device_sync::mojom::BeaconSeedDataView,
                   chromeos::multidevice::BeaconSeed> {
 public:
  static const std::string& data(
      const chromeos::multidevice::BeaconSeed& beacon_seed);
  static base::Time start_time(
      const chromeos::multidevice::BeaconSeed& beacon_seed);
  static base::Time end_time(
      const chromeos::multidevice::BeaconSeed& beacon_seed);

  static bool Read(chromeos::device_sync::mojom::BeaconSeedDataView in,
                   chromeos::multidevice::BeaconSeed* out);
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
  static const std::map<chromeos::multidevice::SoftwareFeature,
                        chromeos::multidevice::SoftwareFeatureState>&
  software_features(const chromeos::multidevice::RemoteDevice& remote_device);
  static const std::vector<chromeos::multidevice::BeaconSeed>& beacon_seeds(
      const chromeos::multidevice::RemoteDevice& remote_device);

  static bool Read(chromeos::device_sync::mojom::RemoteDeviceDataView in,
                   chromeos::multidevice::RemoteDevice* out);
};

template <>
class EnumTraits<chromeos::device_sync::mojom::SoftwareFeature,
                 chromeos::multidevice::SoftwareFeature> {
 public:
  static chromeos::device_sync::mojom::SoftwareFeature ToMojom(
      chromeos::multidevice::SoftwareFeature input);
  static bool FromMojom(chromeos::device_sync::mojom::SoftwareFeature input,
                        chromeos::multidevice::SoftwareFeature* out);
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
