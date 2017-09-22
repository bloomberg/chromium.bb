// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_FAKE_DEVICE_CAPABILITY_MANAGER_H_
#define COMPONENTS_CRYPTAUTH_FAKE_DEVICE_CAPABILITY_MANAGER_H_

#include "base/callback.h"
#include "components/cryptauth/device_capability_manager.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"

namespace cryptauth {

using Capability = DeviceCapabilityManager::Capability;

// Test double for Device Capability Manager.
class FakeDeviceCapabilityManager : public cryptauth::DeviceCapabilityManager {
 public:
  FakeDeviceCapabilityManager();

  ~FakeDeviceCapabilityManager() override;

  void set_find_eligible_devices_for_capability_error_code(
      std::string error_code) {
    find_eligible_devices_for_capability_error_code_ = error_code;
  }

  void set_capability_enabled_error_code(std::string error_code) {
    set_capability_enabled_error_code_ = error_code;
    LOG(ERROR) << set_capability_enabled_error_code_;
  }

  void set_is_capability_promotable_error_code(std::string error_code) {
    is_capability_promotable_error_code_ = error_code;
  }

  void set_external_device_info(
      std::vector<ExternalDeviceInfo> external_device_infos) {
    external_device_infos_ = external_device_infos;
  }

  void set_ineligible_devices(
      std::vector<IneligibleDevice> ineligible_devices) {
    ineligible_devices_ = ineligible_devices;
  }

  // cryptauth::DeviceCapabilityManager:
  void SetCapabilityEnabled(
      const std::string& public_key,
      Capability capability,
      bool enabled,
      const base::Closure& success_callback,
      const base::Callback<void(const std::string&)>& error_callback) override;

  void FindEligibleDevicesForCapability(
      Capability capability,
      const base::Callback<void(const std::vector<ExternalDeviceInfo>&,
                                const std::vector<IneligibleDevice>&)>&
          success_callback,
      const base::Callback<void(const std::string&)>& error_callback) override;

  void IsCapabilityPromotable(
      const std::string& public_key,
      Capability capability,
      const base::Callback<void(bool)>& success_callback,
      const base::Callback<void(const std::string&)>& error_callback) override;

  std::string set_capability_enabled_error_code_;
  std::string find_eligible_devices_for_capability_error_code_;
  std::string is_capability_promotable_error_code_;
  std::vector<ExternalDeviceInfo> external_device_infos_;
  std::vector<IneligibleDevice> ineligible_devices_;
};
}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_FAKE_DEVICE_CAPABILITY_MANAGER_H_
