// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/policy/cloud/cloud_external_data_manager.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_store.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

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
UserCloudPolicyManagerFactory::CreateForOriginalProfile(
    Profile* profile,
    bool force_immediate_load,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  return GetInstance()->CreateManagerForOriginalProfile(
      profile, force_immediate_load, background_task_runner);
}

// static
UserCloudPolicyManager*
UserCloudPolicyManagerFactory::RegisterForOffTheRecordProfile(
    Profile* original_profile,
    Profile* off_the_record_profile) {
  return GetInstance()->RegisterManagerForOffTheRecordProfile(
      original_profile, off_the_record_profile);
}


UserCloudPolicyManagerFactory::UserCloudPolicyManagerFactory()
    : BrowserContextKeyedBaseFactory(
        "UserCloudPolicyManager",
        BrowserContextDependencyManager::GetInstance()) {}

UserCloudPolicyManagerFactory::~UserCloudPolicyManagerFactory() {}

UserCloudPolicyManager* UserCloudPolicyManagerFactory::GetManagerForProfile(
    Profile* profile) {
  // In case |profile| is an incognito Profile, |manager_| will have a matching
  // entry pointing to the PolicyService of the original Profile.
  ManagerMap::const_iterator it = managers_.find(profile);
  return it != managers_.end() ? it->second : NULL;
}

scoped_ptr<UserCloudPolicyManager>
UserCloudPolicyManagerFactory::CreateManagerForOriginalProfile(
    Profile* profile,
    bool force_immediate_load,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  scoped_ptr<UserCloudPolicyStore> store(
      UserCloudPolicyStore::Create(profile->GetPath(), background_task_runner));
  if (force_immediate_load)
    store->LoadImmediately();
  scoped_ptr<UserCloudPolicyManager> manager(
      new UserCloudPolicyManager(profile,
                                 store.Pass(),
                                 scoped_ptr<CloudExternalDataManager>(),
                                 base::MessageLoopProxy::current()));
  manager->Init();
  return manager.Pass();
}

UserCloudPolicyManager*
UserCloudPolicyManagerFactory::RegisterManagerForOffTheRecordProfile(
    Profile* original_profile,
    Profile* off_the_record_profile) {
  // Register the PolicyService of the original Profile for the respective
  // incognito Profile. See GetManagerForProfile above.
  UserCloudPolicyManager* manager = GetManagerForProfile(original_profile);
  Register(off_the_record_profile, manager);
  return manager;
}

void UserCloudPolicyManagerFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  if (profile->IsOffTheRecord())
    return;
  UserCloudPolicyManager* manager = GetManagerForProfile(profile);
  if (manager)
    manager->Shutdown();
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
  // TODO(pneubeck): Re-enable this DCHECK. See http://crbug.com/315266.
  // DCHECK(!entry);
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
