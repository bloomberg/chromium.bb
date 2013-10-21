// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CONFIGURATION_POLICY_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CONFIGURATION_POLICY_HANDLER_CHROMEOS_H_

#include "chrome/browser/extensions/policy_handlers.h"
#include "chrome/browser/policy/configuration_policy_handler.h"
#include "chromeos/network/network_ui_data.h"
#include "components/onc/onc_constants.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace policy {

// ConfigurationPolicyHandler for validation of the network configuration
// policies. These actually don't set any preferences, but the handler just
// generates error messages.
class NetworkConfigurationPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  static NetworkConfigurationPolicyHandler* CreateForUserPolicy();
  static NetworkConfigurationPolicyHandler* CreateForDevicePolicy();

  virtual ~NetworkConfigurationPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;
  virtual void PrepareForDisplaying(PolicyMap* policies) const OVERRIDE;

 private:
  explicit NetworkConfigurationPolicyHandler(
      const char* policy_name,
      ::onc::ONCSource onc_source,
      const char* pref_path);

  // Takes network policy in Value representation and produces an output Value
  // that contains a pretty-printed and sanitized version. In particular, we
  // remove any Passphrases that may be contained in the JSON. Ownership of the
  // return value is transferred to the caller.
  static base::Value* SanitizeNetworkConfig(const base::Value* config);

  // The kind of ONC source that this handler represents. ONCSource
  // distinguishes between user and device policy.
  const ::onc::ONCSource onc_source_;

  // The name of the pref to apply the policy to.
  const char* pref_path_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationPolicyHandler);
};

// Maps the PinnedLauncherApps policy to the corresponding pref.
class PinnedLauncherAppsPolicyHandler
    : public extensions::ExtensionListPolicyHandler {
 public:
  PinnedLauncherAppsPolicyHandler();
  virtual ~PinnedLauncherAppsPolicyHandler();

  // ExtensionListPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PinnedLauncherAppsPolicyHandler);
};

class ScreenMagnifierPolicyHandler : public IntRangePolicyHandlerBase {
 public:
  ScreenMagnifierPolicyHandler();
  virtual ~ScreenMagnifierPolicyHandler();

  // IntRangePolicyHandlerBase:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenMagnifierPolicyHandler);
};

// ConfigurationPolicyHandler for login screen power management settings. This
// does not actually set any prefs, it just checks whether the settings are
// valid and generates errors if appropriate.
class LoginScreenPowerManagementPolicyHandler
    : public TypeCheckingPolicyHandler {
 public:
  LoginScreenPowerManagementPolicyHandler();
  virtual ~LoginScreenPowerManagementPolicyHandler();

  // TypeCheckingPolicyHandler:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenPowerManagementPolicyHandler);
};

// Handles the deprecated IdleAction policy, so both kIdleActionBattery and
// kIdleActionAC fall back to the deprecated action.
class DeprecatedIdleActionHandler : public IntRangePolicyHandlerBase {
 public:
  DeprecatedIdleActionHandler();
  virtual ~DeprecatedIdleActionHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeprecatedIdleActionHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CONFIGURATION_POLICY_HANDLER_CHROMEOS_H_
