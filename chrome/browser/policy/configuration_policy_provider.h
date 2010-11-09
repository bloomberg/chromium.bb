// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_store_interface.h"

namespace policy {

// A mostly-abstract super class for platform-specific policy providers.
// Platform-specific policy providers (Windows Group Policy, gconf,
// etc.) should implement a subclass of this class.
class ConfigurationPolicyProvider {
 public:
  // Used for static arrays of policy values that is used to initialize an
  // instance of the ConfigurationPolicyProvider.
  struct PolicyDefinitionList {
    struct Entry {
      ConfigurationPolicyType policy_type;
      Value::ValueType value_type;
      const char* name;
    };

    const Entry* begin;
    const Entry* end;
  };

  explicit ConfigurationPolicyProvider(const PolicyDefinitionList* policy_list);

  virtual ~ConfigurationPolicyProvider();

  // Must be implemented by provider subclasses to specify the
  // provider-specific policy decisions. The preference service
  // invokes this |Provide| method when it needs a policy
  // provider to specify its policy choices. In |Provide|,
  // the |ConfigurationPolicyProvider| must make calls to the
  // |Apply| method of |store| to apply specific policies.
  // Returns true if the policy could be provided, otherwise false.
  virtual bool Provide(ConfigurationPolicyStoreInterface* store) = 0;

  // Called by the subclass provider at any time to indicate that the currently
  // applied policy is not longer current. A policy refresh will be initiated as
  // soon as possible.
  virtual void NotifyStoreOfPolicyChange();

  // Decodes the value tree and writes the configuration to the given |store|.
  void DecodePolicyValueTree(DictionaryValue* policies,
                             ConfigurationPolicyStoreInterface* store);
 protected:
  const PolicyDefinitionList* policy_definition_list() const {
    return policy_definition_list_;
  }

 private:
  // Contains the default mapping from policy values to the actual names.
  const ConfigurationPolicyProvider::PolicyDefinitionList*
      policy_definition_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyProvider);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_H_
