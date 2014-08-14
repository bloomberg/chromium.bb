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
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/storage/policy_value_store.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/storage/storage_schema_manifest_handler.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/core/common/schema_registry.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/settings_storage_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/value_store/value_store_change.h"
#include "extensions/common/api/storage.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/one_shot_event.h"

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {
class ExtensionRegistry;

namespace storage = core_api::storage;

namespace {

const char kLoadSchemasBackgroundTaskTokenName[] =
    "load_managed_storage_schemas_token";

// The Legacy Browser Support was the first user of the policy-for-extensions
// API, and relied on behavior that will be phased out. If this extension is
// present then its policies will be loaded in a special way.
// TODO(joaodasilva): remove this for M35. http://crbug.com/325349
const char kLegacyBrowserSupportExtensionId[] =
    "heildphpnddilhkemkielfhnkaagiabh";

}  // namespace

// This helper observes initialization of all the installed extensions and
// subsequent loads and unloads, and keeps the SchemaRegistry of the Profile
// in sync with the current list of extensions. This allows the PolicyService
// to fetch cloud policy for those extensions, and allows its providers to
// selectively load only extension policy that has users.
class ManagedValueStoreCache::ExtensionTracker
    : public ExtensionRegistryObserver {
 public:
  explicit ExtensionTracker(Profile* profile);
  virtual ~ExtensionTracker() {}

 private:
  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionWillBeInstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      bool is_update,
      bool from_ephemeral,
      const std::string& old_name) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;

  // Handler for the signal from ExtensionSystem::ready().
  void OnExtensionsReady();

  // Starts a schema load for all extensions that use managed storage.
  void LoadSchemas(scoped_ptr<ExtensionSet> added);

  bool UsesManagedStorage(const Extension* extension) const;

  // Loads the schemas of the |extensions| and passes a ComponentMap to
  // Register().
  static void LoadSchemasOnBlockingPool(scoped_ptr<ExtensionSet> extensions,
                                        base::WeakPtr<ExtensionTracker> self);
  void Register(const policy::ComponentMap* components);

  Profile* profile_;
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;
  policy::SchemaRegistry* schema_registry_;
  base::WeakPtrFactory<ExtensionTracker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTracker);
};

ManagedValueStoreCache::ExtensionTracker::ExtensionTracker(Profile* profile)
    : profile_(profile),
      extension_registry_observer_(this),
      schema_registry_(policy::SchemaRegistryServiceFactory::GetForContext(
                           profile)->registry()),
      weak_factory_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
  // Load schemas when the extension system is ready. It might be ready now.
  ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE,
      base::Bind(&ExtensionTracker::OnExtensionsReady,
                 weak_factory_.GetWeakPtr()));
}

void ManagedValueStoreCache::ExtensionTracker::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    bool from_ephemeral,
    const std::string& old_name) {
  // Some extensions are installed on the first run before the ExtensionSystem
  // becomes ready. Wait until all of them are ready before registering the
  // schemas of managed extensions, so that the policy loaders are reloaded at
  // most once.
  if (!ExtensionSystem::Get(profile_)->ready().is_signaled())
    return;
  scoped_ptr<ExtensionSet> added(new ExtensionSet);
  added->Insert(extension);
  LoadSchemas(added.Pass());
}

void ManagedValueStoreCache::ExtensionTracker::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  if (!ExtensionSystem::Get(profile_)->ready().is_signaled())
    return;
  if (extension && UsesManagedStorage(extension)) {
    schema_registry_->UnregisterComponent(policy::PolicyNamespace(
        policy::POLICY_DOMAIN_EXTENSIONS, extension->id()));
  }
}

void ManagedValueStoreCache::ExtensionTracker::OnExtensionsReady() {
  // Load schemas for all installed extensions.
  LoadSchemas(
      ExtensionRegistry::Get(profile_)->GenerateInstalledExtensionsSet());
}

void ManagedValueStoreCache::ExtensionTracker::LoadSchemas(
    scoped_ptr<ExtensionSet> added) {
  // Filter out extensions that don't use managed storage.
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
      base::Bind(&ExtensionTracker::LoadSchemasOnBlockingPool,
                 base::Passed(&added),
                 weak_factory_.GetWeakPtr()));
}

bool ManagedValueStoreCache::ExtensionTracker::UsesManagedStorage(
    const Extension* extension) const {
  if (extension->manifest()->HasPath(manifest_keys::kStorageManagedSchema))
    return true;

  // TODO(joaodasilva): remove this by M35.
  return extension->id() == kLegacyBrowserSupportExtensionId;
}

// static
void ManagedValueStoreCache::ExtensionTracker::LoadSchemasOnBlockingPool(
    scoped_ptr<ExtensionSet> extensions,
    base::WeakPtr<ExtensionTracker> self) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  scoped_ptr<policy::ComponentMap> components(new policy::ComponentMap);

  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    std::string schema_file;
    if (!(*it)->manifest()->GetString(
            manifest_keys::kStorageManagedSchema, &schema_file)) {
      // TODO(joaodasilva): Remove this. http://crbug.com/325349
      (*components)[(*it)->id()] = policy::Schema();
      continue;
    }
    // The extension should have been validated, so assume the schema exists
    // and is valid.
    std::string error;
    policy::Schema schema =
        StorageSchemaManifestHandler::GetSchema(it->get(), &error);
    // If the schema is invalid then proceed with an empty schema. The extension
    // will be listed in chrome://policy but won't be able to load any policies.
    if (!schema.valid())
      schema = policy::Schema();
    (*components)[(*it)->id()] = schema;
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&ExtensionTracker::Register, self,
                                     base::Owned(components.release())));
}

void ManagedValueStoreCache::ExtensionTracker::Register(
    const policy::ComponentMap* components) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  schema_registry_->RegisterComponents(policy::POLICY_DOMAIN_EXTENSIONS,
                                       *components);

  // The first SetReady() call is performed after the ExtensionSystem is ready,
  // even if there are no managed extensions. It will trigger a loading of the
  // initial policy for any managed extensions, and eventually the PolicyService
  // will become ready for POLICY_DOMAIN_EXTENSIONS, and
  // OnPolicyServiceInitialized() will be invoked.
  // Subsequent calls to SetReady() are ignored.
  schema_registry_->SetReady(policy::POLICY_DOMAIN_EXTENSIONS);
}

ManagedValueStoreCache::ManagedValueStoreCache(
    BrowserContext* context,
    const scoped_refptr<SettingsStorageFactory>& factory,
    const scoped_refptr<SettingsObserverList>& observers)
    : profile_(Profile::FromBrowserContext(context)),
      policy_service_(policy::ProfilePolicyConnectorFactory::GetForProfile(
                          profile_)->policy_service()),
      storage_factory_(factory),
      observers_(observers),
      base_path_(profile_->GetPath().AppendASCII(
          extensions::kManagedSettingsDirectoryName)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  policy_service_->AddObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);

  extension_tracker_.reset(new ExtensionTracker(profile_));

  if (policy_service_->IsInitializationComplete(
          policy::POLICY_DOMAIN_EXTENSIONS)) {
    OnPolicyServiceInitialized(policy::POLICY_DOMAIN_EXTENSIONS);
  }
}

ManagedValueStoreCache::~ManagedValueStoreCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  // Delete the PolicyValueStores on FILE.
  store_map_.clear();
}

void ManagedValueStoreCache::ShutdownOnUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  policy_service_->RemoveObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);
  extension_tracker_.reset();
}

void ManagedValueStoreCache::RunWithValueStoreForExtension(
    const StorageCallback& callback,
    scoped_refptr<const Extension> extension) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  callback.Run(GetStoreFor(extension->id()));
}

void ManagedValueStoreCache::DeleteStorageSoon(
    const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  // It's possible that the store exists, but hasn't been loaded yet
  // (because the extension is unloaded, for example). Open the database to
  // clear it if it exists.
  if (!HasStore(extension_id))
    return;
  GetStoreFor(extension_id)->DeleteStorage();
  store_map_.erase(extension_id);
}

void ManagedValueStoreCache::OnPolicyServiceInitialized(
    policy::PolicyDomain domain) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (domain != policy::POLICY_DOMAIN_EXTENSIONS)
    return;

  // The PolicyService now has all the initial policies ready. Send policy
  // for all the managed extensions to their backing stores now.
  policy::SchemaRegistry* registry =
      policy::SchemaRegistryServiceFactory::GetForContext(profile_)->registry();
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  if (!HasStore(extension_id) && current_policy->empty()) {
    // Don't create the store now if there are no policies configured for this
    // extension. If the extension uses the storage.managed API then the store
    // will be created at RunWithValueStoreForExtension().
    return;
  }

  GetStoreFor(extension_id)->SetCurrentPolicy(*current_policy);
}

PolicyValueStore* ManagedValueStoreCache::GetStoreFor(
    const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

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

bool ManagedValueStoreCache::HasStore(const std::string& extension_id) const {
  // TODO(joaodasilva): move this check to a ValueStore method.
  return base::DirectoryExists(base_path_.AppendASCII(extension_id));
}

}  // namespace extensions
