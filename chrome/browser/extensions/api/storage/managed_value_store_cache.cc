// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/managed_value_store_cache.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/storage/policy_value_store.h"
#include "chrome/browser/extensions/api/storage/settings_storage_factory.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/policy/schema_map.h"
#include "chrome/browser/policy/schema_registry.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/value_store/value_store_change.h"
#include "chrome/common/extensions/api/storage.h"
#include "chrome/common/extensions/api/storage/storage_schema_manifest_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/schema.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission.h"

using content::BrowserThread;

namespace extensions {

namespace storage = api::storage;

namespace {

const char kLoadSchemasBackgroundTaskTokenName[] =
    "load_managed_storage_schemas_token";

}  // namespace

// This helper observes initialization of all the installed extensions and
// subsequent loads and unloads, and keeps the SchemaRegistry of the Profile
// in sync with the current list of extensions. This allows the PolicyService
// to fetch cloud policy for those extensions, and allows its providers to
// selectively load only extension policy that has users.
class ManagedValueStoreCache::ExtensionTracker
    : public content::NotificationObserver {
 public:
  explicit ExtensionTracker(Profile* profile);
  virtual ~ExtensionTracker() {}

  // NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  bool UsesManagedStorage(const Extension* extension) const;

  // Loads the schemas of the |extensions| and passes a ComponentMap to
  // Register().
  static void LoadSchemas(scoped_ptr<ExtensionSet> extensions,
                          base::WeakPtr<ExtensionTracker> self);
  void Register(const policy::ComponentMap* components);

  Profile* profile_;
  content::NotificationRegistrar registrar_;
  policy::SchemaRegistry* schema_registry_;
  base::WeakPtrFactory<ExtensionTracker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTracker);
};

ManagedValueStoreCache::ExtensionTracker::ExtensionTracker(Profile* profile)
    : profile_(profile),
      schema_registry_(
          policy::SchemaRegistryServiceFactory::GetForContext(profile)),
      weak_factory_(this) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSIONS_READY,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
}

void ManagedValueStoreCache::ExtensionTracker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // All the extensions installed on this Profile will be loaded on startup,
  // triggering multiple NOTIFICATION_EXTENSION_LOADED.
  // Wait until all of them are ready before registering the schemas of managed
  // extensions, so that the policy loaders only have to reload at most once.
  if (!ExtensionSystem::Get(profile_)->ready().is_signaled())
    return;

  scoped_ptr<ExtensionSet> added(new ExtensionSet);
  const Extension* removed = NULL;
  switch (type) {
    case chrome::NOTIFICATION_EXTENSIONS_READY:
      added->InsertAll(
          *ExtensionSystem::Get(profile_)->extension_service()->extensions());
      break;
    case chrome::NOTIFICATION_EXTENSION_LOADED:
      added->Insert(content::Details<const Extension>(details).ptr());
      break;
    case chrome::NOTIFICATION_EXTENSION_UNLOADED:
      removed = content::Details<UnloadedExtensionInfo>(details)->extension;
      break;
    default:
      NOTREACHED();
      return;
  }

  if (removed) {
    if (UsesManagedStorage(removed)) {
      schema_registry_->UnregisterComponent(policy::PolicyNamespace(
          policy::POLICY_DOMAIN_EXTENSIONS, removed->id()));
    }
    return;
  }

  ExtensionSet::const_iterator it = added->begin();
  while (it != added->end()) {
    std::string to_remove;
    if (!UsesManagedStorage(*it))
      to_remove = (*it)->id();
    ++it;
    if (!to_remove.empty())
      added->Remove(to_remove);
  }

  // Load the schema files in a background thread.
  BrowserThread::PostBlockingPoolSequencedTask(
      kLoadSchemasBackgroundTaskTokenName, FROM_HERE,
      base::Bind(&ExtensionTracker::LoadSchemas,
                 base::Passed(&added),
                 weak_factory_.GetWeakPtr()));
}

bool ManagedValueStoreCache::ExtensionTracker::UsesManagedStorage(
    const Extension* extension) const {
  if (extension->manifest()->HasPath(manifest_keys::kStorageManagedSchema))
    return true;

  // TODO(joaodasilva): also load extensions that use the storage API for now,
  // to support the Legacy Browser Support extension. Remove this.
  // http://crbug.com/240704
  if (extension->HasAPIPermission(APIPermission::kStorage))
    return true;

  return false;
}

// static
void ManagedValueStoreCache::ExtensionTracker::LoadSchemas(
    scoped_ptr<ExtensionSet> extensions,
    base::WeakPtr<ExtensionTracker> self) {
  scoped_ptr<policy::ComponentMap> components(new policy::ComponentMap);

  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    std::string schema_file;
    if (!(*it)->manifest()->GetString(
            manifest_keys::kStorageManagedSchema, &schema_file)) {
      // TODO(joaodasilva): Remove this. http://crbug.com/240704
      if ((*it)->HasAPIPermission(APIPermission::kStorage)) {
        (*components)[(*it)->id()] = policy::Schema();
      } else {
        NOTREACHED();
      }
      continue;
    }
    // The extension should have been validated, so assume the schema exists
    // and is valid.
    std::string error;
    policy::Schema schema =
        StorageSchemaManifestHandler::GetSchema(it->get(), &error);
    CHECK(schema.valid()) << error;
    (*components)[(*it)->id()] = schema;
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&ExtensionTracker::Register, self,
                                     base::Owned(components.release())));
}

void ManagedValueStoreCache::ExtensionTracker::Register(
    const policy::ComponentMap* components) {
  schema_registry_->RegisterComponents(policy::POLICY_DOMAIN_EXTENSIONS,
                                       *components);

  // The first SetReady() call is performed after receiving
  // NOTIFICATION_EXTENSIONS_READY, even if there are no managed extensions.
  // It will trigger a loading of the initial policy for any managed
  // extensions, and eventually the PolicyService will become ready for
  // POLICY_DOMAIN_EXTENSIONS, and OnPolicyServiceInitialized() will be invoked.
  // Subsequent calls to SetReady() are ignored.
  schema_registry_->SetReady(policy::POLICY_DOMAIN_EXTENSIONS);
}

ManagedValueStoreCache::ManagedValueStoreCache(
    Profile* profile,
    const scoped_refptr<SettingsStorageFactory>& factory,
    const scoped_refptr<SettingsObserverList>& observers)
    : profile_(profile),
      policy_service_(policy::ProfilePolicyConnectorFactory::GetForProfile(
          profile)->policy_service()),
      storage_factory_(factory),
      observers_(observers),
      base_path_(profile->GetPath().AppendASCII(
          extensions::kManagedSettingsDirectoryName)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  policy_service_->AddObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);

  extension_tracker_.reset(new ExtensionTracker(profile_));

  if (policy_service_->IsInitializationComplete(
          policy::POLICY_DOMAIN_EXTENSIONS)) {
    OnPolicyServiceInitialized(policy::POLICY_DOMAIN_EXTENSIONS);
  }
}

ManagedValueStoreCache::~ManagedValueStoreCache() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // Delete the PolicyValueStores on FILE.
  store_map_.clear();
}

void ManagedValueStoreCache::ShutdownOnUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  policy_service_->RemoveObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);
  extension_tracker_.reset();
}

void ManagedValueStoreCache::RunWithValueStoreForExtension(
    const StorageCallback& callback,
    scoped_refptr<const Extension> extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  callback.Run(GetStoreFor(extension->id()));
}

void ManagedValueStoreCache::DeleteStorageSoon(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // It's possible that the store exists, but hasn't been loaded yet
  // (because the extension is unloaded, for example). Open the database to
  // clear it if it exists.
  // TODO(joaodasilva): move this check to a ValueStore method.
  if (!base::DirectoryExists(base_path_.AppendASCII(extension_id)))
    return;
  GetStoreFor(extension_id)->DeleteStorage();
  store_map_.erase(extension_id);
}

void ManagedValueStoreCache::OnPolicyServiceInitialized(
    policy::PolicyDomain domain) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (domain != policy::POLICY_DOMAIN_EXTENSIONS)
    return;

  // The PolicyService now has all the initial policies ready. Send policy
  // for all the managed extensions to their backing stores now.
  policy::SchemaRegistry* registry =
      policy::SchemaRegistryServiceFactory::GetForContext(profile_);
  const policy::ComponentMap* map = registry->schema_map()->GetComponents(
      policy::POLICY_DOMAIN_EXTENSIONS);
  if (!map)
    return;

  const policy::PolicyMap empty_map;
  for (policy::ComponentMap::const_iterator it = map->begin();
       it != map->end(); ++it) {
    const policy::PolicyNamespace ns(policy::POLICY_DOMAIN_EXTENSIONS,
                                     it->first);
    // If there is no policy for |ns| then this will clear the previous store,
    // if there is one.
    OnPolicyUpdated(ns, empty_map, policy_service_->GetPolicies(ns));
  }
}

void ManagedValueStoreCache::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                             const policy::PolicyMap& previous,
                                             const policy::PolicyMap& current) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!policy_service_->IsInitializationComplete(
           policy::POLICY_DOMAIN_EXTENSIONS)) {
    // OnPolicyUpdated is called whenever a policy changes, but it doesn't
    // mean that all the policy providers are ready; wait until we get the
    // final policy values before passing them to the store.
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ManagedValueStoreCache::UpdatePolicyOnFILE,
                 base::Unretained(this),
                 ns.component_id,
                 base::Passed(current.DeepCopy())));
}

void ManagedValueStoreCache::UpdatePolicyOnFILE(
    const std::string& extension_id,
    scoped_ptr<policy::PolicyMap> current_policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  GetStoreFor(extension_id)->SetCurrentPolicy(*current_policy);
}

PolicyValueStore* ManagedValueStoreCache::GetStoreFor(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  PolicyValueStoreMap::iterator it = store_map_.find(extension_id);
  if (it != store_map_.end())
    return it->second.get();

  // Create the store now, and serve the cached policy until the PolicyService
  // sends updated values.
  PolicyValueStore* store = new PolicyValueStore(
      extension_id,
      observers_,
      make_scoped_ptr(storage_factory_->Create(base_path_, extension_id)));
  store_map_[extension_id] = make_linked_ptr(store);

  return store;
}

}  // namespace extensions
