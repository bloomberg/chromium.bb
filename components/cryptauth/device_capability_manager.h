// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_DEVICE_CAPABILITY_MANAGER_H_
#define COMPONENTS_CRYPTAUTH_DEVICE_CAPABILITY_MANAGER_H_

#include "base/callback.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"

namespace cryptauth {

// DeviceCapabilityManager sends requests to the back-end which enable/disable
// device capabilities and finds devices which contain those capabilities. Here,
// the term "capability" refers to the ability of a device to use a given
// feature (e.g. EasyUnlock or Instant Tethering).
class DeviceCapabilityManager {
 public:
  // CAPABILITY_UNLOCK_KEY refers to EasyUnlock.
  enum class Capability { CAPABILITY_UNLOCK_KEY };

  virtual ~DeviceCapabilityManager(){};

  virtual void SetCapabilityEnabled(
      const std::string& public_key,
      Capability capability,
      bool enabled,
      const base::Closure& success_callback,
      const base::Callback<void(const std::string&)>& error_callback) = 0;

  virtual void FindEligibleDevicesForCapability(
      Capability capability,
      const base::Callback<void(const std::vector<ExternalDeviceInfo>&,
                                const std::vector<IneligibleDevice>&)>&
          success_callback,
      const base::Callback<void(const std::string&)>& error_callback) = 0;

  virtual void IsCapabilityPromotable(
      const std::string& public_key,
      Capability capability,
      const base::Callback<void(bool)>& success_callback,
      const base::Callback<void(const std::string&)>& error_callback) = 0;
};
}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_DEVICE_CAPABILITY_MANAGER_H_
