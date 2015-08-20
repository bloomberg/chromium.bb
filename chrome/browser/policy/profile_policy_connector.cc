// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/schema_registry_tracking_policy_provider.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_provider.h"
#include "chrome/browser/chromeos/policy/login_profile_policy_provider.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

namespace policy {

namespace {

std::string GetCloudPolicyManagementDomain(
    const CloudPolicyManager* cloud_policy_manager) {
  const CloudPolicyStore* const store = cloud_policy_manager->core()->store();
  if (store) {
    CHECK(store->is_initialized())
        << "Cloud policy management domain must be "
           "requested only after the policy system is fully initialized";
    if (store->is_managed() && store->policy()->has_username())
      return gaia::ExtractDomainName(store->policy()->username());
  }
  return "";
}

}  // namespace

ProfilePolicyConnector::ProfilePolicyConnector()
#if defined(OS_CHROMEOS)
    : is_primary_user_(false),
      user_cloud_policy_manager_(nullptr)
#else
    : user_cloud_policy_manager_(nullptr)
#endif
{
}

ProfilePolicyConnector::~ProfilePolicyConnector() {}

void ProfilePolicyConnector::Init(
#if defined(OS_CHROMEOS)
    const user_manager::User* user,
#endif
    SchemaRegistry* schema_registry,
    CloudPolicyManager* user_cloud_policy_manager) {
  user_cloud_policy_manager_ = user_cloud_policy_manager;

#if defined(OS_CHROMEOS)
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
#else
  BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
#endif

  if (connector->GetPlatformProvider()) {
    wrapped_platform_policy_provider_.reset(
        new SchemaRegistryTrackingPolicyProvider(
            connector->GetPlatformProvider()));
    wrapped_platform_policy_provider_->Init(schema_registry);
    policy_providers_.push_back(wrapped_platform_policy_provider_.get());
  }

#if defined(OS_CHROMEOS)
  if (connector->GetDeviceCloudPolicyManager())
    policy_providers_.push_back(connector->GetDeviceCloudPolicyManager());
#endif

  if (user_cloud_policy_manager)
    policy_providers_.push_back(user_cloud_policy_manager);

#if defined(OS_CHROMEOS)
  if (!user) {
    DCHECK(schema_registry);
    // This case occurs for the signin profile.
    special_user_policy_provider_.reset(
        new LoginProfilePolicyProvider(connector->GetPolicyService()));
  } else {
    // |user| should never be nullptr except for the signin profile.
    is_primary_user_ =
        user == user_manager::UserManager::Get()->GetPrimaryUser();
    // Note that |DeviceLocalAccountPolicyProvider::Create| returns nullptr when
    // the user supplied is not a device-local account user.
    special_user_policy_provider_ = DeviceLocalAccountPolicyProvider::Create(
        user->email(),
        connector->GetDeviceLocalAccountPolicyService());
  }
  if (special_user_policy_provider_) {
    special_user_policy_provider_->Init(schema_registry);
    policy_providers_.push_back(special_user_policy_provider_.get());
  }
#endif

  policy_service_.reset(new PolicyServiceImpl(policy_providers_));

#if defined(OS_CHROMEOS)
  if (is_primary_user_) {
    if (user_cloud_policy_manager)
      connector->SetUserPolicyDelegate(user_cloud_policy_manager);
    else if (special_user_policy_provider_)
      connector->SetUserPolicyDelegate(special_user_policy_provider_.get());
  }
#endif
}

void ProfilePolicyConnector::InitForTesting(scoped_ptr<PolicyService> service) {
  policy_service_ = service.Pass();
}

void ProfilePolicyConnector::OverrideIsManagedForTesting(bool is_managed) {
  is_managed_override_.reset(new bool(is_managed));
}

void ProfilePolicyConnector::Shutdown() {
#if defined(OS_CHROMEOS)
  if (is_primary_user_) {
    BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    connector->SetUserPolicyDelegate(nullptr);
  }
  if (special_user_policy_provider_)
    special_user_policy_provider_->Shutdown();
#endif
  if (wrapped_platform_policy_provider_)
    wrapped_platform_policy_provider_->Shutdown();
}

bool ProfilePolicyConnector::IsManaged() const {
  if (is_managed_override_)
    return *is_managed_override_;
  return !GetManagementDomain().empty();
}

std::string ProfilePolicyConnector::GetManagementDomain() const {
  if (user_cloud_policy_manager_)
    return GetCloudPolicyManagementDomain(user_cloud_policy_manager_);
#if defined(OS_CHROMEOS)
  if (special_user_policy_provider_) {
    // |special_user_policy_provider_| is non-null for device-local accounts and
    // for the login profile.
    // They receive policy iff the device itself is managed.
    const DeviceCloudPolicyManagerChromeOS* const device_cloud_policy_manager =
        g_browser_process->platform_part()
            ->browser_policy_connector_chromeos()
            ->GetDeviceCloudPolicyManager();
    // The device_cloud_policy_manager can be a nullptr in unit tests.
    if (device_cloud_policy_manager)
      return GetCloudPolicyManagementDomain(device_cloud_policy_manager);
  }
#endif
  return "";
}

bool ProfilePolicyConnector::IsPolicyFromCloudPolicy(const char* name) const {
  const ConfigurationPolicyProvider* const provider =
      DeterminePolicyProviderForPolicy(name);
  return provider == user_cloud_policy_manager_;
}

const ConfigurationPolicyProvider*
ProfilePolicyConnector::DeterminePolicyProviderForPolicy(
    const char* name) const {
  const PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, "");
  for (const ConfigurationPolicyProvider* provider : policy_providers_) {
    if (provider->policies().Get(chrome_ns).Get(name))
      return provider;
  }
  return nullptr;
}

}  // namespace policy
