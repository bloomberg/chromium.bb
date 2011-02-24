// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_MAC_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_MAC_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/policy/configuration_policy_store_interface.h"
#include "chrome/browser/policy/file_based_policy_provider.h"
#include "chrome/browser/preferences_mac.h"

namespace policy {

// A provider delegate implementation that reads Mac OS X's managed preferences.
class MacPreferencesPolicyProviderDelegate
    : public FileBasedPolicyProvider::ProviderDelegate {
 public:
  // Takes ownership of |preferences|.
  MacPreferencesPolicyProviderDelegate(
      MacPreferences* preferences,
      const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list);
  virtual ~MacPreferencesPolicyProviderDelegate();

  // FileBasedPolicyLoader::Delegate implementation.
  virtual DictionaryValue* Load();
  virtual base::Time GetLastModification();

 private:
  // In order to access the application preferences API, the names and values of
  // the policies that are recognized must be known to the loader.
  // Unfortunately, we cannot get the policy list at load time from the
  // provider, because the loader may outlive the provider, so we store our own
  // pointer to the list.
  const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list_;

  scoped_ptr<MacPreferences> preferences_;

  DISALLOW_COPY_AND_ASSIGN(MacPreferencesPolicyProviderDelegate);
};

// An implementation of |ConfigurationPolicyProvider| using the mechanism
// provided by Mac OS X's managed preferences.
class ConfigurationPolicyProviderMac
    : public FileBasedPolicyProvider {
 public:
  explicit ConfigurationPolicyProviderMac(
      const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list);
  // For testing; takes ownership of |preferences|.
  ConfigurationPolicyProviderMac(
      const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list,
      MacPreferences* preferences);

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyProviderMac);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_MAC_H_
