// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_STUB_INSTALL_ATTRIBUTES_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_STUB_INSTALL_ATTRIBUTES_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/settings/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace chromeos {

// This class allows tests to override specific values for easier testing.
// To make IsEnterpriseDevice() return true, set a non-empty registration
// user.
class StubInstallAttributes : public InstallAttributes {
 public:
  StubInstallAttributes();

  void SetDomain(const std::string& domain);
  void SetRegistrationUser(const std::string& user);
  void SetDeviceId(const std::string& id);
  void SetMode(policy::DeviceMode mode);

 private:
  DISALLOW_COPY_AND_ASSIGN(StubInstallAttributes);
};

// Helper class to set install attributes in the scope of a test.
class ScopedStubInstallAttributes {
 public:
  ScopedStubInstallAttributes(const std::string& domain,
                              const std::string& registration_user,
                              const std::string& device_id,
                              policy::DeviceMode mode);
  ~ScopedStubInstallAttributes();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_STUB_INSTALL_ATTRIBUTES_H_
