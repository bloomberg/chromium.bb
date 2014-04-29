// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/stub_enterprise_install_attributes.h"

#include <string>

#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace policy {

StubEnterpriseInstallAttributes::StubEnterpriseInstallAttributes()
    : EnterpriseInstallAttributes(NULL) {
  device_locked_ = true;
}

void StubEnterpriseInstallAttributes::SetDomain(const std::string& domain) {
  registration_domain_ = domain;
}

void StubEnterpriseInstallAttributes::SetRegistrationUser(
    const std::string& user) {
  registration_user_ = user;
}

void StubEnterpriseInstallAttributes::SetDeviceId(const std::string& id) {
  registration_device_id_ = id;
}

void StubEnterpriseInstallAttributes::SetMode(DeviceMode mode) {
  registration_mode_ = mode;
}

ScopedStubEnterpriseInstallAttributes::ScopedStubEnterpriseInstallAttributes(
    const std::string& domain,
    const std::string& registration_user,
    const std::string& device_id,
    DeviceMode mode) {
  StubEnterpriseInstallAttributes* attributes =
      new StubEnterpriseInstallAttributes();
  attributes->SetDomain(domain);
  attributes->SetRegistrationUser(registration_user);
  attributes->SetDeviceId(device_id);
  attributes->SetMode(mode);
  BrowserPolicyConnectorChromeOS::SetInstallAttributesForTesting(attributes);
}

ScopedStubEnterpriseInstallAttributes::
~ScopedStubEnterpriseInstallAttributes() {
  BrowserPolicyConnectorChromeOS::RemoveInstallAttributesForTesting();
}

}  // namespace policy
