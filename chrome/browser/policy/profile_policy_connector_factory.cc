// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector_factory.h"

#include "base/logging.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#endif
#endif

namespace policy {

// static
ProfilePolicyConnectorFactory* ProfilePolicyConnectorFactory::GetInstance() {
  return Singleton<ProfilePolicyConnectorFactory>::get();
}

// static
ProfilePolicyConnector* ProfilePolicyConnectorFactory::GetForProfile(
    Profile* profile) {
  return GetInstance()->GetForProfileInternal(profile);
}

// static
scoped_ptr<ProfilePolicyConnector>
    ProfilePolicyConnectorFactory::CreateForProfile(
        Profile* profile,
        bool force_immediate_load,
        base::SequencedTaskRunner* sequenced_task_runner) {
  return GetInstance()->CreateForProfileInternal(
      profile, force_immediate_load, sequenced_task_runner);
}

void ProfilePolicyConnectorFactory::SetServiceForTesting(
    Profile* profile,
    ProfilePolicyConnector* connector) {
  ProfilePolicyConnector*& map_entry = connectors_[profile];
  CHECK(!map_entry);
  map_entry = connector;
}

ProfilePolicyConnectorFactory::ProfilePolicyConnectorFactory()
    : ProfileKeyedBaseFactory("ProfilePolicyConnector",
                              ProfileDependencyManager::GetInstance()) {
#if defined(ENABLE_CONFIGURATION_POLICY)
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
    ProfilePolicyConnectorFactory::GetForProfileInternal(Profile* profile) {
  // Get the connector for the original Profile, so that the incognito Profile
  // gets managed settings from the same PolicyService.
  ConnectorMap::const_iterator it =
      connectors_.find(profile->GetOriginalProfile());
  CHECK(it != connectors_.end());
  return it->second;
}

scoped_ptr<ProfilePolicyConnector>
    ProfilePolicyConnectorFactory::CreateForProfileInternal(
        Profile* profile,
        bool force_immediate_load,
        base::SequencedTaskRunner* sequenced_task_runner) {
  DCHECK(connectors_.find(profile) == connectors_.end());
  ProfilePolicyConnector* connector = new ProfilePolicyConnector(profile);
  connector->Init(force_immediate_load, sequenced_task_runner);
  connectors_[profile] = connector;
  return scoped_ptr<ProfilePolicyConnector>(connector);
}

void ProfilePolicyConnectorFactory::ProfileShutdown(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  if (profile->IsOffTheRecord())
    return;
  ConnectorMap::iterator it = connectors_.find(profile);
  if (it != connectors_.end())
    it->second->Shutdown();
}

void ProfilePolicyConnectorFactory::ProfileDestroyed(
    content::BrowserContext* context) {
  ConnectorMap::iterator it = connectors_.find(static_cast<Profile*>(context));
  if (it != connectors_.end())
    connectors_.erase(it);
  ProfileKeyedBaseFactory::ProfileDestroyed(context);
}

void ProfilePolicyConnectorFactory::RegisterUserPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if defined(OS_CHROMEOS)
  registry->RegisterBooleanPref(
      prefs::kUsedPolicyCertificatesOnce,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
}

void ProfilePolicyConnectorFactory::SetEmptyTestingFactory(
    content::BrowserContext* context) {}

void ProfilePolicyConnectorFactory::CreateServiceNow(
    content::BrowserContext* context) {}

}  // namespace policy
