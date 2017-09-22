// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/fake_device_capability_manager.h"

namespace cryptauth {

FakeDeviceCapabilityManager::FakeDeviceCapabilityManager() {}

FakeDeviceCapabilityManager::~FakeDeviceCapabilityManager() {}

void FakeDeviceCapabilityManager::SetCapabilityEnabled(
    const std::string& public_key,
    Capability capability,
    bool enabled,
    const base::Closure& success_callback,
    const base::Callback<void(const std::string&)>& error_callback) {
  if (set_capability_enabled_error_code_.empty()) {
    success_callback.Run();
    return;
  }
  error_callback.Run(std::move(set_capability_enabled_error_code_));
}

void FakeDeviceCapabilityManager::FindEligibleDevicesForCapability(
    Capability capability,
    const base::Callback<void(const std::vector<ExternalDeviceInfo>&,
                              const std::vector<IneligibleDevice>&)>&
        success_callback,
    const base::Callback<void(const std::string&)>& error_callback) {
  if (find_eligible_devices_for_capability_error_code_.empty()) {
    success_callback.Run(external_device_infos_, ineligible_devices_);
    return;
  }
  error_callback.Run(find_eligible_devices_for_capability_error_code_);
}

void FakeDeviceCapabilityManager::IsCapabilityPromotable(
    const std::string& public_key,
    Capability capability,
    const base::Callback<void(bool)>& success_callback,
    const base::Callback<void(const std::string&)>& error_callback) {
  if (is_capability_promotable_error_code_.empty()) {
    success_callback.Run(true /* capability is promotable */);
    return;
  }
  error_callback.Run(is_capability_promotable_error_code_);
}

}  // namespace cryptauth