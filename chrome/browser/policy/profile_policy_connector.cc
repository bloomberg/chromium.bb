// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/forwarding_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_provider.h"
#include "chrome/browser/chromeos/policy/login_profile_policy_provider.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

namespace policy {

namespace {

bool HasChromePolicy(ConfigurationPolicyProvider* provider,
                     const char* name) {
  if (!provider)
    return false;
  PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, "");
  return provider->policies().Get(chrome_ns).Get(name) != NULL;
}

}  // namespace

ProfilePolicyConnector::ProfilePolicyConnector()
#if defined(OS_CHROMEOS)
    : is_primary_user_(false),
      user_cloud_policy_manager_(NULL)
#else
    : user_cloud_policy_manager_(NULL)
#endif
      {}

ProfilePolicyConnector::~ProfilePolicyConnector() {}

void ProfilePolicyConnector::Init(
    bool force_immediate_load,
#if defined(OS_CHROMEOS)
    const user_manager::User* user,
#endif
    SchemaRegistry* schema_registry,
    CloudPolicyManager* user_cloud_policy_manager) {
  user_cloud_policy_manager_ = user_cloud_policy_manager;

  // |providers| contains a list of the policy providers available for the
  // PolicyService of this connector, in decreasing order of priority.
  //
  // Note: all the providers appended to this vector must eventually become
  // initialized for every policy domain, otherwise some subsystems will never
  // use the policies exposed by the PolicyService!
  // The default ConfigurationPolicyProvider::IsInitializationComplete()
  // result is true, so take care if a provider overrides that.
  //
  // Note: if you append a new provider then make sure IsPolicyFromCloudPolicy()
  // is also updated below.
  std::vector<ConfigurationPolicyProvider*> providers;

#if defined(OS_CHROMEOS)
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
#else
  BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
#endif

  if (connector->GetPlatformProvider()) {
    forwarding_policy_provider_.reset(
        new ForwardingPolicyProvider(connector->GetPlatformProvider()));
    forwarding_policy_provider_->Init(schema_registry);
    providers.push_back(forwarding_policy_provider_.get());
  }

#if defined(OS_CHROMEOS)
  if (connector->GetDeviceCloudPolicyManager())
    providers.push_back(connector->GetDeviceCloudPolicyManager());
#endif

  if (user_cloud_policy_manager)
    providers.push_back(user_cloud_policy_manager);

#if defined(OS_CHROMEOS)
  if (!user) {
    DCHECK(schema_registry);
    // This case occurs for the signin profile.
    special_user_policy_provider_.reset(
        new LoginProfilePolicyProvider(connector->GetPolicyService()));
  } else {
    // |user| should never be NULL except for the signin profile.
    is_primary_user_ =
        user == user_manager::UserManager::Get()->GetPrimaryUser();
    special_user_policy_provider_ = DeviceLocalAccountPolicyProvider::Create(
        user->email(),
        connector->GetDeviceLocalAccountPolicyService());
  }
  if (special_user_policy_provider_) {
    special_user_policy_provider_->Init(schema_registry);
    providers.push_back(special_user_policy_provider_.get());
  }
#endif

  policy_service_.reset(new PolicyServiceImpl(providers));

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

void ProfilePolicyConnector::Shutdown() {
#if defined(OS_CHROMEOS)
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (is_primary_user_)
    connector->SetUserPolicyDelegate(NULL);
  if (special_user_policy_provider_)
    special_user_policy_provider_->Shutdown();
#endif
  if (forwarding_policy_provider_)
    forwarding_policy_provider_->Shutdown();
}

bool ProfilePolicyConnector::IsManaged() const {
  return !GetManagementDomain().empty();
}

std::string ProfilePolicyConnector::GetManagementDomain() const {
  if (!user_cloud_policy_manager_)
    return "";
  CloudPolicyStore* store = user_cloud_policy_manager_->core()->store();
  if (store && store->is_managed() && store->policy()->has_username())
    return gaia::ExtractDomainName(store->policy()->username());
  return "";
}

bool ProfilePolicyConnector::IsPolicyFromCloudPolicy(const char* name) const {
  if (!HasChromePolicy(user_cloud_policy_manager_, name))
    return false;

  // Check all the providers that have higher priority than the
  // |user_cloud_policy_manager_|. These checks must be kept in sync with the
  // order of the providers in Init().

  if (HasChromePolicy(forwarding_policy_provider_.get(), name))
    return false;

#if defined(OS_CHROMEOS)
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (HasChromePolicy(connector->GetDeviceCloudPolicyManager(), name))
    return false;
#endif

  return true;
}

}  // namespace policy
