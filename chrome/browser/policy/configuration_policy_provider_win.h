// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_WIN_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_WIN_H_
#pragma once

#include "base/string16.h"
#include "chrome/browser/policy/asynchronous_policy_provider.h"

namespace policy {

// An implementation of |ConfigurationPolicyProvider| using the
// mechanism provided by Windows Groups Policy. Policy decisions are
// stored as values in a special section of the Windows Registry.
// On a managed machine in a domain, this portion of the registry is
// periodically updated by the Windows Group Policy machinery to contain
// the latest version of the policy set by administrators.
class ConfigurationPolicyProviderWin : public AsynchronousPolicyProvider {
 public:
  ConfigurationPolicyProviderWin(const PolicyDefinitionList* policy_list,
                                 const string16& registry_key);
  virtual ~ConfigurationPolicyProviderWin() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyProviderWin);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_WIN_H_
