// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace user_manager {
class User;
}

namespace policy {

class CloudPolicyManager;
class ConfigurationPolicyProvider;
class PolicyService;
class SchemaRegistry;

// A KeyedService that creates and manages the per-Profile policy
// components.
class ProfilePolicyConnector : public KeyedService {
 public:
  ProfilePolicyConnector();
  ~ProfilePolicyConnector() override;

  void Init(
#if defined(OS_CHROMEOS)
      const user_manager::User* user,
#endif
      SchemaRegistry* schema_registry,
      CloudPolicyManager* user_cloud_policy_manager);

  void InitForTesting(scoped_ptr<PolicyService> service);
  void OverrideIsManagedForTesting(bool is_managed);

  // KeyedService:
  void Shutdown() override;

  // This is never NULL.
  PolicyService* policy_service() const { return policy_service_.get(); }

  // Returns true if this Profile is under cloud policy management. You must
  // call this method only when the policies system is fully initialized.
  bool IsManaged() const;

  // Returns the cloud policy management domain, if this Profile is under
  // cloud policy management. Otherwise returns an empty string. You must call
  // this method only when the policies system is fully initialized.
  std::string GetManagementDomain() const;

  // Returns true if the |name| Chrome user policy is currently set via the
  // CloudPolicyManager and isn't being overridden by a higher-level provider.
  bool IsPolicyFromCloudPolicy(const char* name) const;

 private:
  // Find the policy provider that provides the |name| Chrome policy, if any. In
  // case of multiple providers sharing the same policy, the one with the
  // highest priority will be returned.
  const ConfigurationPolicyProvider* DeterminePolicyProviderForPolicy(
      const char* name) const;

#if defined(ENABLE_CONFIGURATION_POLICY)
#if defined(OS_CHROMEOS)
  // Some of the user policy configuration affects browser global state, and
  // can only come from one Profile. |is_primary_user_| is true if this
  // connector belongs to the first signed-in Profile, and in that case that
  // Profile's policy is the one that affects global policy settings in
  // local state.
  bool is_primary_user_;

  scoped_ptr<ConfigurationPolicyProvider> special_user_policy_provider_;
#endif  // defined(OS_CHROMEOS)

  scoped_ptr<ConfigurationPolicyProvider> wrapped_platform_policy_provider_;
  CloudPolicyManager* user_cloud_policy_manager_;
#endif  // defined(ENABLE_CONFIGURATION_POLICY)

  // |policy_providers_| contains a list of the policy providers available for
  // the PolicyService of this connector, in decreasing order of priority.
  //
  // Note: All the providers appended to this vector must eventually become
  // initialized for every policy domain, otherwise some subsystems will never
  // use the policies exposed by the PolicyService!
  // The default ConfigurationPolicyProvider::IsInitializationComplete()
  // result is true, so take care if a provider overrides that.
  std::vector<ConfigurationPolicyProvider*> policy_providers_;

  scoped_ptr<PolicyService> policy_service_;
  scoped_ptr<bool> is_managed_override_;

  DISALLOW_COPY_AND_ASSIGN(ProfilePolicyConnector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_H_
