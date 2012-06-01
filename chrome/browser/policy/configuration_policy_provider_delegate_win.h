// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  explicit ConfigurationPolicyProviderDelegateWin(
      const PolicyDefinitionList* policy_definition_list);
  virtual ~ConfigurationPolicyProviderDelegateWin() {}

  // AsynchronousPolicyProvider::Delegate overrides:
  virtual scoped_ptr<PolicyBundle> Load() OVERRIDE;

 private:
  const PolicyDefinitionList* policy_definition_list_;

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyProviderDelegateWin);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_DELEGATE_WIN_H_
