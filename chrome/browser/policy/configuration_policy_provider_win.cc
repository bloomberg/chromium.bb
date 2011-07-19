// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider_win.h"

#include "chrome/browser/policy/asynchronous_policy_provider.h"
#include "chrome/browser/policy/configuration_policy_loader_win.h"
#include "chrome/browser/policy/configuration_policy_provider_delegate_win.h"

namespace policy {

// Period at which to run the reload task in case the group policy change
// watchers fail.
const int kReloadIntervalMinutes = 15;

ConfigurationPolicyProviderWin::ConfigurationPolicyProviderWin(
    const PolicyDefinitionList* policy_list)
    : AsynchronousPolicyProvider(
          policy_list,
          new ConfigurationPolicyLoaderWin(
              new ConfigurationPolicyProviderDelegateWin(policy_list),
              kReloadIntervalMinutes)) {}

}  // namespace policy
