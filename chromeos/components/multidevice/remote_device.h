// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_H_
#define CHROMEOS_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_H_

#include <map>
#include <string>
#include <vector>

#include "chromeos/components/multidevice/software_feature_state.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"

namespace chromeos {

namespace multidevice {

struct RemoteDevice {
 public:
  // Generates the device ID for a device given its public key.
  static std::string GenerateDeviceId(const std::string& public_key);

  std::string user_id;
  std::string name;
  std::string public_key;
  std::string persistent_symmetric_key;
  int64_t last_update_time_millis;
  std::map<cryptauth::SoftwareFeature, SoftwareFeatureState> software_features;
  std::vector<cryptauth::BeaconSeed> beacon_seeds;

  RemoteDevice();
  RemoteDevice(const std::string& user_id,
               const std::string& name,
               const std::string& public_key,
               const std::string& persistent_symmetric_key,
               int64_t last_update_time_millis,
               const std::map<cryptauth::SoftwareFeature, SoftwareFeatureState>&
                   software_features,
               const std::vector<cryptauth::BeaconSeed>& beacon_seeds);
  RemoteDevice(const RemoteDevice& other);
  ~RemoteDevice();

  std::string GetDeviceId() const;

  bool operator==(const RemoteDevice& other) const;

  // Compares devices via their public keys. Note that this function is
  // necessary in order to use |RemoteDevice| as a key of a std::map.
  bool operator<(const RemoteDevice& other) const;
};

typedef std::vector<RemoteDevice> RemoteDeviceList;

}  // namespace multidevice

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MULTIDEVICE_REMOTE_DEVICE_H_
