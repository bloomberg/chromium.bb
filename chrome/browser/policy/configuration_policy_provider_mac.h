// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_MAC_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_MAC_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/policy/configuration_policy_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/preferences_mac.h"

namespace policy {

// An implementation of |ConfigurationPolicyProvider| using the mechanism
// provided by Mac OS X's managed preferences.
class ConfigurationPolicyProviderMac : public ConfigurationPolicyProvider {
 public:
  explicit ConfigurationPolicyProviderMac(
      const StaticPolicyValueMap& policy_map);
  // For testing; takes ownership of |preferences|.
  ConfigurationPolicyProviderMac(const StaticPolicyValueMap& policy_map,
                                 MacPreferences* preferences);
  virtual ~ConfigurationPolicyProviderMac() { }

  // ConfigurationPolicyProvider method overrides:
  virtual bool Provide(ConfigurationPolicyStore* store);

 protected:
  scoped_ptr<MacPreferences> preferences_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_MAC_H_
