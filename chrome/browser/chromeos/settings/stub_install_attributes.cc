// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/stub_install_attributes.h"

#include <string>

#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace chromeos {

StubInstallAttributes::StubInstallAttributes() : InstallAttributes(nullptr) {
  device_locked_ = true;
}

void StubInstallAttributes::SetDomain(const std::string& domain) {
  registration_domain_ = domain;
}

void StubInstallAttributes::SetRegistrationUser(const std::string& user) {
  registration_user_ = user;
}

void StubInstallAttributes::SetDeviceId(const std::string& id) {
  registration_device_id_ = id;
}

void StubInstallAttributes::SetMode(policy::DeviceMode mode) {
  registration_mode_ = mode;
}

ScopedStubInstallAttributes::ScopedStubInstallAttributes(
    const std::string& domain,
    const std::string& registration_user,
    const std::string& device_id,
    policy::DeviceMode mode) {
  StubInstallAttributes* attributes = new StubInstallAttributes();
  attributes->SetDomain(domain);
  attributes->SetRegistrationUser(registration_user);
  attributes->SetDeviceId(device_id);
  attributes->SetMode(mode);
  policy::BrowserPolicyConnectorChromeOS::SetInstallAttributesForTesting(
      attributes);
}

ScopedStubInstallAttributes::~ScopedStubInstallAttributes() {
  policy::BrowserPolicyConnectorChromeOS::RemoveInstallAttributesForTesting();
}

}  // namespace chromeos
