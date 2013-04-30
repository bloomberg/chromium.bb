// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/chrome_switches.h"

namespace policy {

// static
UserCloudPolicyManagerFactory* UserCloudPolicyManagerFactory::GetInstance() {
  return Singleton<UserCloudPolicyManagerFactory>::get();
}

// static
UserCloudPolicyManager* UserCloudPolicyManagerFactory::GetForProfile(
    Profile* profile) {
  return GetInstance()->GetManagerForProfile(profile);
}

// static
scoped_ptr<UserCloudPolicyManager>
    UserCloudPolicyManagerFactory::CreateForProfile(Profile* profile,
                                                    bool force_immediate_load) {
  return GetInstance()->CreateManagerForProfile(profile, force_immediate_load);
}

UserCloudPolicyManagerFactory::UserCloudPolicyManagerFactory()
    : ProfileKeyedBaseFactory("UserCloudPolicyManager",
                              ProfileDependencyManager::GetInstance()) {}

UserCloudPolicyManagerFactory::~UserCloudPolicyManagerFactory() {}

UserCloudPolicyManager* UserCloudPolicyManagerFactory::GetManagerForProfile(
    Profile* profile) {
  // Get the manager for the original profile, since the PolicyService is
  // also shared between the incognito Profile and the original Profile.
  ManagerMap::const_iterator it = managers_.find(profile->GetOriginalProfile());
  return it != managers_.end() ? it->second : NULL;
}

scoped_ptr<UserCloudPolicyManager>
    UserCloudPolicyManagerFactory::CreateManagerForProfile(
        Profile* profile,
        bool force_immediate_load) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableCloudPolicyOnSignin)) {
    return scoped_ptr<UserCloudPolicyManager>();
  }
  scoped_ptr<UserCloudPolicyStore> store(UserCloudPolicyStore::Create(profile));
  if (force_immediate_load)
    store->LoadImmediately();
  scoped_ptr<UserCloudPolicyManager> manager(
      new UserCloudPolicyManager(profile, store.Pass()));
  manager->Init();
  return manager.Pass();
}

void UserCloudPolicyManagerFactory::ProfileShutdown(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  if (profile->IsOffTheRecord())
    return;
  UserCloudPolicyManager* manager = GetManagerForProfile(profile);
  if (manager) {
    manager->CloudPolicyManager::Shutdown();
    manager->ProfileKeyedService::Shutdown();
  }
}

void UserCloudPolicyManagerFactory::SetEmptyTestingFactory(
    content::BrowserContext* profile) {
}

void UserCloudPolicyManagerFactory::CreateServiceNow(
    content::BrowserContext* profile) {
}

void UserCloudPolicyManagerFactory::Register(Profile* profile,
                                             UserCloudPolicyManager* instance) {
  UserCloudPolicyManager*& entry = managers_[profile];
  DCHECK(!entry);
  entry = instance;
}

void UserCloudPolicyManagerFactory::Unregister(
    Profile* profile,
    UserCloudPolicyManager* instance) {
  ManagerMap::iterator entry = managers_.find(profile);
  if (entry != managers_.end()) {
    DCHECK_EQ(instance, entry->second);
    managers_.erase(entry);
  } else {
    NOTREACHED();
  }
}

}  // namespace policy
