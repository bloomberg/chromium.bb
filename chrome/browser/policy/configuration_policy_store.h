// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_STORE_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_STORE_H_
#pragma once

#include "base/basictypes.h"

class Value;

// An abstract super class for policy stores that provides a method that can be
// called by a |ConfigurationPolicyProvider| to specify a policy.
class ConfigurationPolicyStore {
 public:
  ConfigurationPolicyStore() {}
  virtual ~ConfigurationPolicyStore() {}

  enum PolicyType {
    kPolicyHomePage,
    kPolicyHomepageIsNewTabPage,
    kPolicyRestoreOnStartup,
    kPolicyURLsToRestoreOnStartup,
    kPolicyProxyServerMode,
    kPolicyProxyServer,
    kPolicyProxyPacUrl,
    kPolicyProxyBypassList,
    kPolicyAlternateErrorPagesEnabled,
    kPolicySearchSuggestEnabled,
    kPolicyDnsPrefetchingEnabled,
    kPolicySafeBrowsingEnabled,
    kPolicyMetricsReportingEnabled,
    kPolicyPasswordManagerEnabled,
    kPolicyAutoFillEnabled,
    kPolicySyncDisabled,
    kPolicyApplicationLocale,
    kPolicyExtensionInstallAllowList,
    kPolicyExtensionInstallDenyList,
    kPolicyShowHomeButton,
    kPolicyDisabledPlugins
  };

  static const int kPolicyNoProxyServerMode = 0;
  static const int kPolicyAutoDetectProxyMode = 1;
  static const int kPolicyManuallyConfiguredProxyMode = 2;
  static const int kPolicyUseSystemProxyMode = 3;

  // A |ConfigurationPolicyProvider| specifies the value of a policy setting
  // through a call to |Apply|.
  // The configuration policy pref store takes over the ownership of |value|.
  virtual void Apply(PolicyType policy, Value* value) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyStore);
};

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_STORE_H_
