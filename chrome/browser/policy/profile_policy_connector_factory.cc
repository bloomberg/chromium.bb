// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector_factory.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/policy_service.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "components/policy/core/common/policy_service_impl.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif
#endif

namespace policy {

// static
ProfilePolicyConnectorFactory* ProfilePolicyConnectorFactory::GetInstance() {
  return Singleton<ProfilePolicyConnectorFactory>::get();
}

// static
ProfilePolicyConnector* ProfilePolicyConnectorFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return GetInstance()->GetForBrowserContextInternal(context);
}

// static
scoped_ptr<ProfilePolicyConnector>
ProfilePolicyConnectorFactory::CreateForBrowserContext(
    content::BrowserContext* context,
    bool force_immediate_load) {
  return GetInstance()->CreateForBrowserContextInternal(context,
                                                        force_immediate_load);
}

void ProfilePolicyConnectorFactory::SetServiceForTesting(
    content::BrowserContext* context,
    ProfilePolicyConnector* connector) {
  ProfilePolicyConnector*& map_entry = connectors_[context];
  CHECK(!map_entry);
  map_entry = connector;
}

#if defined(ENABLE_CONFIGURATION_POLICY)
void ProfilePolicyConnectorFactory::PushProviderForTesting(
    ConfigurationPolicyProvider* provider) {
  test_providers_.push_back(provider);
}
#endif

ProfilePolicyConnectorFactory::ProfilePolicyConnectorFactory()
    : BrowserContextKeyedBaseFactory(
        "ProfilePolicyConnector",
        BrowserContextDependencyManager::GetInstance()) {
#if defined(ENABLE_CONFIGURATION_POLICY)
  DependsOn(SchemaRegistryServiceFactory::GetInstance());
#if defined(OS_CHROMEOS)
  DependsOn(UserCloudPolicyManagerFactoryChromeOS::GetInstance());
#else
  DependsOn(UserCloudPolicyManagerFactory::GetInstance());
#endif
#endif
}

ProfilePolicyConnectorFactory::~ProfilePolicyConnectorFactory() {
  DCHECK(connectors_.empty());
}

ProfilePolicyConnector*
ProfilePolicyConnectorFactory::GetForBrowserContextInternal(
    content::BrowserContext* context) {
  // Get the connector for the original Profile, so that the incognito Profile
  // gets managed settings from the same PolicyService.
  content::BrowserContext* const original_context =
      chrome::GetBrowserContextRedirectedInIncognito(context);
  const ConnectorMap::const_iterator it = connectors_.find(original_context);
  CHECK(it != connectors_.end());
  return it->second;
}

scoped_ptr<ProfilePolicyConnector>
ProfilePolicyConnectorFactory::CreateForBrowserContextInternal(
    content::BrowserContext* context,
    bool force_immediate_load) {
  DCHECK(connectors_.find(context) == connectors_.end());

  SchemaRegistry* schema_registry = NULL;
  CloudPolicyManager* user_cloud_policy_manager = NULL;

#if defined(ENABLE_CONFIGURATION_POLICY)
  schema_registry =
      SchemaRegistryServiceFactory::GetForContext(context)->registry();

#if defined(OS_CHROMEOS)
  Profile* const profile = Profile::FromBrowserContext(context);
  const user_manager::User* user = NULL;
  if (!chromeos::ProfileHelper::IsSigninProfile(profile)) {
    user = chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
    CHECK(user);
  }
  user_cloud_policy_manager =
      UserCloudPolicyManagerFactoryChromeOS::GetForProfile(profile);
#else
  user_cloud_policy_manager =
      UserCloudPolicyManagerFactory::GetForBrowserContext(context);
#endif  // defined(OS_CHROMEOS)
#endif  // defined(ENABLE_CONFIGURATION_POLICY)

  scoped_ptr<ProfilePolicyConnector> connector(new ProfilePolicyConnector());

#if defined(ENABLE_CONFIGURATION_POLICY)
  if (test_providers_.empty()) {
    connector->Init(force_immediate_load,
#if defined(OS_CHROMEOS)
                    user,
#endif
                    schema_registry,
                    user_cloud_policy_manager);
  } else {
    PolicyServiceImpl::Providers providers;
    providers.push_back(test_providers_.front());
    test_providers_.pop_front();
    scoped_ptr<PolicyService> service(new PolicyServiceImpl(providers));
    connector->InitForTesting(service.Pass());
  }
#else
  connector->Init(false, NULL, NULL);
#endif

  connectors_[context] = connector.get();
  return connector.Pass();
}

void ProfilePolicyConnectorFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  if (Profile::FromBrowserContext(context)->IsOffTheRecord())
    return;
  const ConnectorMap::const_iterator it = connectors_.find(context);
  if (it != connectors_.end())
    it->second->Shutdown();
}

void ProfilePolicyConnectorFactory::BrowserContextDestroyed(
    content::BrowserContext* context) {
  const ConnectorMap::iterator it = connectors_.find(context);
  if (it != connectors_.end())
    connectors_.erase(it);
  BrowserContextKeyedBaseFactory::BrowserContextDestroyed(context);
}

void ProfilePolicyConnectorFactory::SetEmptyTestingFactory(
    content::BrowserContext* context) {}

bool ProfilePolicyConnectorFactory::HasTestingFactory(
    content::BrowserContext* context) {
  return false;
}

void ProfilePolicyConnectorFactory::CreateServiceNow(
    content::BrowserContext* context) {}

}  // namespace policy
