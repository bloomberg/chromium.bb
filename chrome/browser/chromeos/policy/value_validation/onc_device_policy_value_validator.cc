// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/value_validation/onc_device_policy_value_validator.h"

#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

namespace em = enterprise_management;

namespace policy {

ONCDevicePolicyValueValidator::ONCDevicePolicyValueValidator()
    : ONCPolicyValueValidatorBase<em::ChromeDeviceSettingsProto>(
          key::kDeviceOpenNetworkConfiguration,
          ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY) {}

base::Optional<std::string>
ONCDevicePolicyValueValidator::GetONCStringFromPayload(
    const em::ChromeDeviceSettingsProto& policy_payload) const {
  if (policy_payload.has_open_network_configuration() &&
      policy_payload.open_network_configuration()
          .has_open_network_configuration()) {
    return policy_payload.open_network_configuration()
        .open_network_configuration();
  }
  return base::nullopt;
}

}  // namespace policy
