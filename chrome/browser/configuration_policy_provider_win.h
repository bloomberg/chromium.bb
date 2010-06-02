// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFIGURATION_POLICY_PROVIDER_WIN_H_
#define CHROME_BROWSER_CONFIGURATION_POLICY_PROVIDER_WIN_H_

#include "chrome/browser/configuration_policy_store.h"
#include "chrome/browser/configuration_policy_provider.h"

// An implementation of |ConfigurationPolicyProvider| using the
// mechanism provided by Windows Groups Policy. Policy decisions are
// stored as values in a special section of the Windows Registry.
// On a managed machine in a domain, this portion of the registry is
// periodically updated by the Windows Group Policy machinery to contain
// the latest version of the policy set by administrators.
class ConfigurationPolicyProviderWin : public ConfigurationPolicyProvider {
 public:
  ConfigurationPolicyProviderWin();
  virtual ~ConfigurationPolicyProviderWin() { }

  // ConfigurationPolicyProvider method overrides:
  virtual bool Provide(ConfigurationPolicyStore* store);

 protected:
  // The sub key path for Chromium's Group Policy information in the
  // Windows registry.
  static const wchar_t kPolicyRegistrySubKey[];

 private:
  // Methods to perfrom type-specific policy lookups in the registry.
  // HKLM is checked first, then HKCU.
  bool GetRegistryPolicyString(const wchar_t* value_name, string16* result);
  bool GetRegistryPolicyBoolean(const wchar_t* value_name, bool* result);
  bool GetRegistryPolicyInteger(const wchar_t* value_name, uint32* result);
};

#endif  // CHROME_BROWSER_CONFIGURATION_POLICY_PROVIDER_WIN_H_

