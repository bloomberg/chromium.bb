// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_DELEGATE_WIN_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_DELEGATE_WIN_H_
#pragma once

#include "base/string16.h"
#include "chrome/browser/policy/asynchronous_policy_provider.h"

namespace policy {

class ConfigurationPolicyProviderDelegateWin
    : public AsynchronousPolicyProvider::Delegate {
 public:
  ConfigurationPolicyProviderDelegateWin(
      const PolicyDefinitionList* policy_definition_list,
      const string16& registry_key);
  virtual ~ConfigurationPolicyProviderDelegateWin() {}

  // AsynchronousPolicyProvider::Delegate overrides:
  virtual DictionaryValue* Load() OVERRIDE;

 private:
  // Methods to perform type-specific policy lookups in the registry.
  // HKLM is checked first, then HKCU.

  // Reads a string registry value |name| at the specified |key| and puts the
  // resulting string in |result|.
  bool GetRegistryPolicyString(const string16& name, string16* result) const;
  // Gets a list value contained under |key| one level below the policy root.
  bool GetRegistryPolicyStringList(const string16& key,
                                   ListValue* result) const;
  bool GetRegistryPolicyBoolean(const string16& value_name,
                                bool* result) const;
  bool GetRegistryPolicyInteger(const string16& value_name,
                                uint32* result) const;

  const PolicyDefinitionList* policy_definition_list_;
  const string16 registry_key_;

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyProviderDelegateWin);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_DELEGATE_WIN_H_
