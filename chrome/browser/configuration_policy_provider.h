// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFIGURATION_POLICY_PROVIDER_H_
#define CHROME_BROWSER_CONFIGURATION_POLICY_PROVIDER_H_

#include "base/basictypes.h"

class ConfigurationPolicyStore;
class DictionaryValue;

// An abstract super class for platform-specific policy providers.
// Platform-specific policy providers (Windows Group Policy, gconf,
// etc.) should implement a subclass of this class.
class ConfigurationPolicyProvider {
 public:
  ConfigurationPolicyProvider() {}
  virtual ~ConfigurationPolicyProvider() {}

  // Must be implemented by provider subclasses to specify the
  // provider-specific policy decisions. The preference service
  // invokes this |Provide| method when it needs a policy
  // provider to specify its policy choices. In |Provide|,
  // the |ConfigurationPolicyProvider| must make calls to the
  // |Apply| method of |store| to apply specific policies.
  // Returns true if the policy could be provided, otherwise false.
  virtual bool Provide(ConfigurationPolicyStore* store) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyProvider);
};

// Base functionality for all policy providers that use a DictionaryValue tree
// structure holding key-value pairs for storing policy settings. Decodes the
// value tree and writes the configuration to the given |store|.
void DecodePolicyValueTree(DictionaryValue* policies,
                           ConfigurationPolicyStore* store);

#endif  // CHROME_BROWSER_CONFIGURATION_POLICY_PROVIDER_H_

