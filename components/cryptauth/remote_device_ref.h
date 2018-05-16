// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_REF_H_
#define COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_REF_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/software_feature_state.h"

namespace cryptauth {

// Contains metadata specific to a device associated with a user's account.
// Because this metadata contains large and expensive data types,
// RemoteDeviceRef is implemented using a pointer to a struct containing this
// metadata; if multiple clients want to reference the same device, multiple
// RemoteDeviceRefs can be created cheaply without duplicating the underlying
// data. Should be passed by value.
class RemoteDeviceRef {
 public:
  RemoteDeviceRef(RemoteDeviceRef& other);
  ~RemoteDeviceRef();

  const std::string& user_id() const { return remote_device_->user_id; }
  const std::string& name() const { return remote_device_->name; }
  const std::string& public_key() const { return remote_device_->public_key; }
  const std::string& persistent_symmetric_key() const {
    return remote_device_->persistent_symmetric_key;
  }
  bool unlock_key() const { return remote_device_->unlock_key; }
  bool supports_mobile_hotspot() const {
    return remote_device_->supports_mobile_hotspot;
  }
  int64_t last_update_time_millis() const {
    return remote_device_->last_update_time_millis;
  }
  const std::vector<BeaconSeed>& beacon_seeds() const {
    return remote_device_->beacon_seeds;
  }

  std::string GetDeviceId() const;
  SoftwareFeatureState GetSoftwareFeatureState(
      const SoftwareFeature& software_feature) const;

  // Returns a shortened device ID for the purpose of concise logging (device
  // IDs are often so long that logs are difficult to read). Note that this
  // ID is not guaranteed to be unique, so it should only be used for log.
  std::string GetTruncatedDeviceIdForLogs() const;

  bool operator==(const RemoteDeviceRef& other) const;

  // This function is necessary in order to use |RemoteDeviceRef| as a key of a
  // std::map.
  bool operator<(const RemoteDeviceRef& other) const;

 private:
  friend class RemoteDeviceRefTest;
  FRIEND_TEST_ALL_PREFIXES(RemoteDeviceRefTest, TestFields);
  FRIEND_TEST_ALL_PREFIXES(RemoteDeviceRefTest, TestCopyAndAssign);

  RemoteDeviceRef(std::shared_ptr<RemoteDevice> remote_device);

  std::shared_ptr<const RemoteDevice> remote_device_;
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_REF_H_