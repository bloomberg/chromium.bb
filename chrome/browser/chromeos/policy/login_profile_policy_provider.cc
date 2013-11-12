// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/login_profile_policy_provider.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/login_screen_power_management_policy.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_types.h"
#include "policy/policy_constants.h"

namespace policy {

namespace {

// Applies the value of |device_policy| in |device_policy_map| as the
// recommended value of |user_policy| in |user_policy_map|. If the value of
// |device_policy| is unset, does nothing.
void ApplyDevicePolicyAsRecommendedPolicy(const std::string& device_policy,
                                          const std::string& user_policy,
                                          const PolicyMap& device_policy_map,
                                          PolicyMap* user_policy_map) {
  const base::Value* value = device_policy_map.GetValue(device_policy);
  if (value) {
    user_policy_map->Set(user_policy,
                         POLICY_LEVEL_RECOMMENDED,
                         POLICY_SCOPE_USER,
                         value->DeepCopy(),
                         NULL);
  }
}

// Applies |value| as the mandatory value of |user_policy| in |user_policy_map|.
// If |value| is NULL, does nothing.
void ApplyValueAsMandatoryPolicy(const base::Value* value,
                                 const std::string& user_policy,
                                 PolicyMap* user_policy_map) {
  if (value) {
    user_policy_map->Set(user_policy,
                         POLICY_LEVEL_MANDATORY,
                         POLICY_SCOPE_USER,
                         value->DeepCopy(),
                         NULL);
  }
}

}  // namespace

LoginProfilePolicyProvider::LoginProfilePolicyProvider(
    PolicyService* device_policy_service)
    : device_policy_service_(device_policy_service),
      waiting_for_device_policy_refresh_(false),
      weak_factory_(this) {
}

LoginProfilePolicyProvider::~LoginProfilePolicyProvider() {
}

void LoginProfilePolicyProvider::Init(SchemaRegistry* registry) {
  ConfigurationPolicyProvider::Init(registry);
  device_policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
  if (device_policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME))
    UpdateFromDevicePolicy();
}

void LoginProfilePolicyProvider::Shutdown() {
  device_policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
  weak_factory_.InvalidateWeakPtrs();
  ConfigurationPolicyProvider::Shutdown();
}

void LoginProfilePolicyProvider::RefreshPolicies() {
  waiting_for_device_policy_refresh_ = true;
  weak_factory_.InvalidateWeakPtrs();
  device_policy_service_->RefreshPolicies(base::Bind(
      &LoginProfilePolicyProvider::OnDevicePolicyRefreshDone,
      weak_factory_.GetWeakPtr()));
}

void LoginProfilePolicyProvider::OnPolicyUpdated(const PolicyNamespace& ns,
                                                 const PolicyMap& previous,
                                                 const PolicyMap& current) {
  if (ns == PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
    UpdateFromDevicePolicy();
}

void LoginProfilePolicyProvider::OnPolicyServiceInitialized(
    PolicyDomain domain) {
  if (domain == POLICY_DOMAIN_CHROME)
    UpdateFromDevicePolicy();
}

void LoginProfilePolicyProvider::OnDevicePolicyRefreshDone() {
  waiting_for_device_policy_refresh_ = false;
  UpdateFromDevicePolicy();
}

void LoginProfilePolicyProvider::UpdateFromDevicePolicy() {
  // If a policy refresh is in progress, wait for it to finish.
  if (waiting_for_device_policy_refresh_)
    return;

  const PolicyNamespace chrome_namespaces(POLICY_DOMAIN_CHROME, std::string());
  const PolicyMap& device_policy_map =
      device_policy_service_->GetPolicies(chrome_namespaces);
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle);
  PolicyMap& user_policy_map = bundle->Get(chrome_namespaces);

  ApplyDevicePolicyAsRecommendedPolicy(
      key::kDeviceLoginScreenDefaultLargeCursorEnabled,
      key::kLargeCursorEnabled,
      device_policy_map, &user_policy_map);
  ApplyDevicePolicyAsRecommendedPolicy(
      key::kDeviceLoginScreenDefaultSpokenFeedbackEnabled,
      key::kSpokenFeedbackEnabled,
      device_policy_map, &user_policy_map);
  ApplyDevicePolicyAsRecommendedPolicy(
      key::kDeviceLoginScreenDefaultHighContrastEnabled,
      key::kHighContrastEnabled,
      device_policy_map, &user_policy_map);
  ApplyDevicePolicyAsRecommendedPolicy(
      key::kDeviceLoginScreenDefaultScreenMagnifierType,
      key::kScreenMagnifierType,
      device_policy_map, &user_policy_map);

  // TODO(bartfab): Consolidate power management user policies into a single
  // JSON policy, allowing the value of the device policy to be simply forwarded
  // here. http://crbug.com/258339
  const base::Value* value =
      device_policy_map.GetValue(key::kDeviceLoginScreenPowerManagement);
  std::string json;
  if (value && value->GetAsString(&json)) {
    LoginScreenPowerManagementPolicy power_management_policy;
    power_management_policy.Init(json, NULL);
    ApplyValueAsMandatoryPolicy(power_management_policy.GetScreenDimDelayAC(),
                                key::kScreenDimDelayAC,
                                &user_policy_map);
    ApplyValueAsMandatoryPolicy(power_management_policy.GetScreenOffDelayAC(),
                                key::kScreenOffDelayAC,
                                &user_policy_map);
    ApplyValueAsMandatoryPolicy(power_management_policy.GetIdleDelayAC(),
                                key::kIdleDelayAC,
                                &user_policy_map);
    ApplyValueAsMandatoryPolicy(
        power_management_policy.GetScreenDimDelayBattery(),
        key::kScreenDimDelayBattery,
        &user_policy_map);
    ApplyValueAsMandatoryPolicy(
        power_management_policy.GetScreenOffDelayBattery(),
        key::kScreenOffDelayBattery,
        &user_policy_map);
    ApplyValueAsMandatoryPolicy(power_management_policy.GetIdleDelayBattery(),
                                key::kIdleDelayBattery,
                                &user_policy_map);
    ApplyValueAsMandatoryPolicy(power_management_policy.GetIdleActionAC(),
                                key::kIdleActionAC,
                                &user_policy_map);
    ApplyValueAsMandatoryPolicy(power_management_policy.GetIdleActionBattery(),
                                key::kIdleActionBattery,
                                &user_policy_map);
    ApplyValueAsMandatoryPolicy(power_management_policy.GetLidCloseAction(),
                                key::kLidCloseAction,
                                &user_policy_map);
    ApplyValueAsMandatoryPolicy(
        power_management_policy.GetUserActivityScreenDimDelayScale(),
        key::kUserActivityScreenDimDelayScale,
        &user_policy_map);
  }

  UpdatePolicy(bundle.Pass());
}

}  // namespace policy
