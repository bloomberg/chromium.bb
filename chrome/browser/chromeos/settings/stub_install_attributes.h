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

// This class allows tests to set specific configurations for testing.
class StubInstallAttributes : public InstallAttributes {
 public:
  StubInstallAttributes();

  // Setup as not-yet enrolled.
  void Clear();

  // Setup as consumer device.  (Clears existing configuration.)
  void SetConsumer();

  // Setup as enterprise enrolled.  (Clears existing configuration.)
  void SetEnterprise(const std::string& domain, const std::string& device_id);

 private:
  DISALLOW_COPY_AND_ASSIGN(StubInstallAttributes);
};

// Helper class to set install attributes in the scope of a test.
class ScopedStubInstallAttributes {
 public:
  ~ScopedStubInstallAttributes();

  // Factory for empty (unset) ScopedStubInstallAttributes.
  static ScopedStubInstallAttributes CreateUnset();

  // Factory for consumer-type ScopedStubInstallAttributes.
  static ScopedStubInstallAttributes CreateConsumer();

  // Factory for enterprise-type ScopedStubInstallAttributes.
  static ScopedStubInstallAttributes CreateEnterprise(
      const std::string& domain,
      const std::string& device_id);

 private:
  ScopedStubInstallAttributes();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_STUB_INSTALL_ATTRIBUTES_H_
