// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store.h"
#include "content/public/browser/browser_context.h"

namespace policy {

namespace {

// Directory inside the profile directory where policy-related resources are
// stored.
const base::FilePath::CharType kPolicy[] = FILE_PATH_LITERAL("Policy");

// Directory under kPolicy, in the user's profile dir, where policy for
// components is cached.
const base::FilePath::CharType kComponentsDir[] =
    FILE_PATH_LITERAL("Components");

}  // namespace

// A BrowserContextKeyedService that wraps a UserCloudPolicyManager.
class UserCloudPolicyManagerFactory::ManagerWrapper
    : public BrowserContextKeyedService {
 public:
  explicit ManagerWrapper(UserCloudPolicyManager* manager)
      : manager_(manager) {}
  virtual ~ManagerWrapper() {}

  virtual void Shutdown() OVERRIDE {
    manager_->Shutdown();
  }

  UserCloudPolicyManager* manager() { return manager_; }

 private:
  UserCloudPolicyManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(ManagerWrapper);
};

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
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& file_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner) {
  return GetInstance()->CreateManagerForOriginalBrowserContext(
      context,
      force_immediate_load,
      background_task_runner,
      file_task_runner,
      io_task_runner);
}

// static
UserCloudPolicyManager*
UserCloudPolicyManagerFactory::RegisterForOffTheRecordBrowserContext(
    content::BrowserContext* original_context,
    content::BrowserContext* off_the_record_context) {
  return GetInstance()->RegisterManagerForOffTheRecordBrowserContext(
      original_context, off_the_record_context);
}

void UserCloudPolicyManagerFactory::RegisterForTesting(
    content::BrowserContext* context,
    UserCloudPolicyManager* manager) {
  ManagerWrapper*& manager_wrapper = manager_wrappers_[context];
  delete manager_wrapper;
  manager_wrapper = new ManagerWrapper(manager);
}

UserCloudPolicyManagerFactory::UserCloudPolicyManagerFactory()
    : BrowserContextKeyedBaseFactory(
        "UserCloudPolicyManager",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SchemaRegistryServiceFactory::GetInstance());
}

UserCloudPolicyManagerFactory::~UserCloudPolicyManagerFactory() {
  DCHECK(manager_wrappers_.empty());
}

UserCloudPolicyManager*
UserCloudPolicyManagerFactory::GetManagerForBrowserContext(
    content::BrowserContext* context) {
  // In case |context| is an incognito Profile/Context, |manager_wrappers_|
  // will have a matching entry pointing to the manager of the original context.
  ManagerWrapperMap::const_iterator it = manager_wrappers_.find(context);
  return it != manager_wrappers_.end() ? it->second->manager() : NULL;
}

scoped_ptr<UserCloudPolicyManager>
UserCloudPolicyManagerFactory::CreateManagerForOriginalBrowserContext(
    content::BrowserContext* context,
    bool force_immediate_load,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& file_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner) {
  DCHECK(!context->IsOffTheRecord());

  scoped_ptr<UserCloudPolicyStore> store(
      UserCloudPolicyStore::Create(context->GetPath(), background_task_runner));
  if (force_immediate_load)
    store->LoadImmediately();

  const base::FilePath component_policy_cache_dir =
      context->GetPath().Append(kPolicy).Append(kComponentsDir);

  scoped_ptr<UserCloudPolicyManager> manager(
      new UserCloudPolicyManager(store.Pass(),
                                 component_policy_cache_dir,
                                 scoped_ptr<CloudExternalDataManager>(),
                                 base::MessageLoopProxy::current(),
                                 file_task_runner,
                                 io_task_runner));
  manager->Init(SchemaRegistryServiceFactory::GetForContext(context));
  manager_wrappers_[context] = new ManagerWrapper(manager.get());
  return manager.Pass();
}

UserCloudPolicyManager*
UserCloudPolicyManagerFactory::RegisterManagerForOffTheRecordBrowserContext(
    content::BrowserContext* original_context,
    content::BrowserContext* off_the_record_context) {
  // Register the UserCloudPolicyManager of the original context for the
  // respective incognito context. See also GetManagerForBrowserContext.
  UserCloudPolicyManager* manager =
      GetManagerForBrowserContext(original_context);
  manager_wrappers_[off_the_record_context] = new ManagerWrapper(manager);
  return manager;
}

void UserCloudPolicyManagerFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  if (context->IsOffTheRecord())
    return;
  ManagerWrapperMap::iterator it = manager_wrappers_.find(context);
  // E.g. for a TestingProfile there might not be a manager created.
  if (it != manager_wrappers_.end())
    it->second->Shutdown();
}

void UserCloudPolicyManagerFactory::BrowserContextDestroyed(
    content::BrowserContext* context) {
  ManagerWrapperMap::iterator it = manager_wrappers_.find(context);
  if (it != manager_wrappers_.end()) {
    // The manager is not owned by the factory, so it's not deleted here.
    delete it->second;
    manager_wrappers_.erase(it);
  }
}

void UserCloudPolicyManagerFactory::SetEmptyTestingFactory(
    content::BrowserContext* context) {}

void UserCloudPolicyManagerFactory::CreateServiceNow(
    content::BrowserContext* context) {}

}  // namespace policy
