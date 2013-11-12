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
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace policy {

// static
UserCloudPolicyManagerFactory* UserCloudPolicyManagerFactory::GetInstance() {
  return Singleton<UserCloudPolicyManagerFactory>::get();
}

// static
UserCloudPolicyManager* UserCloudPolicyManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return GetInstance()->GetManagerForBrowserContext(context);
}

// static
scoped_ptr<UserCloudPolicyManager>
UserCloudPolicyManagerFactory::CreateForOriginalBrowserContext(
    content::BrowserContext* context,
    bool force_immediate_load,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  return GetInstance()->CreateManagerForOriginalBrowserContext(
      context, force_immediate_load, background_task_runner);
}

// static
UserCloudPolicyManager*
UserCloudPolicyManagerFactory::RegisterForOffTheRecordBrowserContext(
    content::BrowserContext* original_context,
    content::BrowserContext* off_the_record_context) {
  return GetInstance()->RegisterManagerForOffTheRecordBrowserContext(
      original_context, off_the_record_context);
}


UserCloudPolicyManagerFactory::UserCloudPolicyManagerFactory()
    : BrowserContextKeyedBaseFactory(
        "UserCloudPolicyManager",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SchemaRegistryServiceFactory::GetInstance());
}

UserCloudPolicyManagerFactory::~UserCloudPolicyManagerFactory() {}

UserCloudPolicyManager*
UserCloudPolicyManagerFactory::GetManagerForBrowserContext(
    content::BrowserContext* context) {
  // In case |context| is an incognito Profile/Context, |manager_| will have a
  // matching entry pointing to the PolicyService of the original context.
  ManagerMap::const_iterator it = managers_.find(context);
  return it != managers_.end() ? it->second : NULL;
}

scoped_ptr<UserCloudPolicyManager>
UserCloudPolicyManagerFactory::CreateManagerForOriginalBrowserContext(
    content::BrowserContext* context,
    bool force_immediate_load,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  DCHECK(!context->IsOffTheRecord());
  scoped_ptr<UserCloudPolicyStore> store(
      UserCloudPolicyStore::Create(context->GetPath(), background_task_runner));
  if (force_immediate_load)
    store->LoadImmediately();
  scoped_ptr<UserCloudPolicyManager> manager(
      new UserCloudPolicyManager(context,
                                 store.Pass(),
                                 scoped_ptr<CloudExternalDataManager>(),
                                 base::MessageLoopProxy::current()));
  manager->Init(SchemaRegistryServiceFactory::GetForContext(context));
  return manager.Pass();
}

UserCloudPolicyManager*
UserCloudPolicyManagerFactory::RegisterManagerForOffTheRecordBrowserContext(
    content::BrowserContext* original_context,
    content::BrowserContext* off_the_record_context) {
  // Register the PolicyService of the original context for the respective
  // incognito context. See also GetManagerForBrowserContext.
  UserCloudPolicyManager* manager =
      GetManagerForBrowserContext(original_context);
  Register(off_the_record_context, manager);
  return manager;
}

void UserCloudPolicyManagerFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  UserCloudPolicyManager* manager = GetManagerForBrowserContext(context);
  // E.g. for a TestingProfile there might not be a manager created.
  if (!manager)
    return;

  // Remove |context| from the |managers_| map. In case of |context| is
  // off the record, don't delete the manager instance. In case of the original
  // context, also Shutdown the manager instance (which calls Unregister as
  // well). We never delete managers here, because they're not owned by this
  // factory.
  if (context->IsOffTheRecord())
    Unregister(context, NULL /* don't check the manager instance */);
  else
    manager->Shutdown();
}

void UserCloudPolicyManagerFactory::SetEmptyTestingFactory(
    content::BrowserContext* context) {
}

void UserCloudPolicyManagerFactory::CreateServiceNow(
    content::BrowserContext* context) {
}

void UserCloudPolicyManagerFactory::Register(content::BrowserContext* context,
                                             UserCloudPolicyManager* instance) {
  UserCloudPolicyManager*& entry = managers_[context];
  DCHECK(!entry);
  entry = instance;
}

void UserCloudPolicyManagerFactory::Unregister(
    content::BrowserContext* context,
    UserCloudPolicyManager* instance) {
  ManagerMap::iterator entry = managers_.find(context);
  if (entry != managers_.end()) {
    if (instance)
      DCHECK_EQ(instance, entry->second);
    managers_.erase(entry);
  } else {
    NOTREACHED();
  }
}

}  // namespace policy
