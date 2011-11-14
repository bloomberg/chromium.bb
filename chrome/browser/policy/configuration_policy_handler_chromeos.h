// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_CHROMEOS_H_
#pragma once

#include "chrome/browser/policy/configuration_policy_handler.h"

namespace policy {

// ConfigurationPolicyHandler for validation of the network configuration
// policies. These actually don't set any preferences, but the handler just
// generates error messages.
class NetworkConfigurationPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  explicit NetworkConfigurationPolicyHandler(ConfigurationPolicyType type);
  virtual ~NetworkConfigurationPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;
  virtual void PrepareForDisplaying(PolicyMap* policies) const OVERRIDE;

 private:
  // Takes network policy in Value representation and produces an output Value
  // that contains a pretty-printed and sanitized version. In particular, we
  // remove any Passphrases that may be contained in the JSON. Ownership of the
  // return value is transferred to the caller.
  static Value* SanitizeNetworkConfig(const Value* config);

  // Filters a network dictionary to remove all sensitive fields and replace
  // their values with placeholders.
  static void StripSensitiveValues(DictionaryValue* network_dict);

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_CHROMEOS_H_
