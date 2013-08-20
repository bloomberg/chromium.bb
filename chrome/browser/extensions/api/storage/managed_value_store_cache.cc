// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/managed_value_store_cache.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/storage/policy_value_store.h"
#include "chrome/browser/extensions/api/storage/settings_storage_factory.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/policy/policy_domain_descriptor.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/value_store/value_store_change.h"
#include "chrome/common/extensions/api/storage.h"
#include "chrome/common/extensions/api/storage/storage_schema_manifest_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "components/policy/core/common/policy_schema.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/manifest_constants.h"

using content::BrowserThread;

namespace extensions {

namespace storage = api::storage;

namespace {

const char kLoadSchemasBackgroundTaskTokenName[] =
    "load_managed_storage_schemas_token";

}  // namespace

// This helper observes initialization of all the installed extensions and
// subsequent loads and unloads, and keeps the PolicyService of the Profile
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
  // Loads the schemas of the |extensions| and passes a PolicyDomainDescriptor
  // to RegisterDomain().
  static void LoadSchemas(scoped_ptr<ExtensionSet> extensions,
                          base::WeakPtr<ExtensionTracker> self);
  void RegisterDomain(
      scoped_refptr<const policy::PolicyDomainDescriptor> descriptor);

  Profile* profile_;
  content::NotificationRegistrar registrar_;
  base::WeakPtrFactory<ExtensionTracker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTracker);
};

ManagedValueStoreCache::ExtensionTracker::ExtensionTracker(Profile* profile)
    : profile_(profile),
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
  if (!ExtensionSystem::Get(profile_)->ready().is_signaled())
    return;

  scoped_refptr<policy::PolicyDomainDescriptor> descriptor(
      new policy::PolicyDomainDescriptor(policy::POLICY_DOMAIN_EXTENSIONS));
  const ExtensionSet* set =
      ExtensionSystem::Get(profile_)->extension_service()->extensions();
  scoped_ptr<ExtensionSet> managed_extensions(new ExtensionSet());
  for (ExtensionSet::const_iterator it = set->begin(); it != set->end(); ++it) {
    if ((*it)->manifest()->HasPath(manifest_keys::kStorageManagedSchema)) {
      managed_extensions->Insert(*it);
    }

    // TODO(joaodasilva): also load extensions that use the storage API for now,
    // to support the Legacy Browser Support extension. Remove this for M30.
    // http://crbug.com/240704
    if ((*it)->HasAPIPermission(APIPermission::kStorage))
      managed_extensions->Insert(*it);
  }

  // Load the schema files in a background thread.
  BrowserThread::PostBlockingPoolSequencedTask(
      kLoadSchemasBackgroundTaskTokenName, FROM_HERE,
      base::Bind(&ExtensionTracker::LoadSchemas,
                 base::Passed(&managed_extensions),
                 weak_factory_.GetWeakPtr()));
}

// static
void ManagedValueStoreCache::ExtensionTracker::LoadSchemas(
    scoped_ptr<ExtensionSet> extensions,
    base::WeakPtr<ExtensionTracker> self) {
  scoped_refptr<policy::PolicyDomainDescriptor> descriptor =
      new policy::PolicyDomainDescriptor(policy::POLICY_DOMAIN_EXTENSIONS);

  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    std::string schema_file;
    if (!(*it)->manifest()->GetString(
            manifest_keys::kStorageManagedSchema, &schema_file)) {
      // TODO(joaodasilva): Remove this for M30. http://crbug.com/240704
      if ((*it)->HasAPIPermission(APIPermission::kStorage)) {
        descriptor->RegisterComponent((*it)->id(),
                                      scoped_ptr<policy::PolicySchema>());
      } else {
        NOTREACHED();
      }
      continue;
    }
    // The extension should have been validated, so assume the schema exists
    // and is valid.
    std::string error;
    scoped_ptr<policy::PolicySchema> schema =
        StorageSchemaManifestHandler::GetSchema(it->get(), &error);
    CHECK(schema) << error;
    descriptor->RegisterComponent((*it)->id(), schema.Pass());
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ExtensionTracker::RegisterDomain, self, descriptor));
}

void ManagedValueStoreCache::ExtensionTracker::RegisterDomain(
    scoped_refptr<const policy::PolicyDomainDescriptor> descriptor) {
  policy::ProfilePolicyConnector* connector =
      policy::ProfilePolicyConnectorFactory::GetForProfile(profile_);
  connector->policy_service()->RegisterPolicyDomain(descriptor);
}

ManagedValueStoreCache::ManagedValueStoreCache(
    Profile* profile,
    const scoped_refptr<SettingsStorageFactory>& factory,
    const scoped_refptr<SettingsObserverList>& observers)
    : weak_factory_(this),
      weak_this_on_ui_(weak_factory_.GetWeakPtr()),
      profile_(profile),
      event_router_(ExtensionSystem::Get(profile)->event_router()),
      storage_factory_(factory),
      observers_(observers),
      base_path_(profile->GetPath().AppendASCII(
          ExtensionService::kManagedSettingsDirectoryName)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // |event_router_| can be NULL on unit_tests.
  if (event_router_)
    event_router_->RegisterObserver(this, storage::OnChanged::kEventName);

  GetPolicyService()->AddObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);

  extension_tracker_.reset(new ExtensionTracker(profile_));
}

ManagedValueStoreCache::~ManagedValueStoreCache() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!event_router_);
  // Delete the PolicyValueStores on FILE.
  store_map_.clear();
}

void ManagedValueStoreCache::ShutdownOnUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GetPolicyService()->RemoveObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);
  if (event_router_)
    event_router_->UnregisterObserver(this);
  event_router_ = NULL;
  weak_factory_.InvalidateWeakPtrs();
  extension_tracker_.reset();
}

void ManagedValueStoreCache::RunWithValueStoreForExtension(
    const StorageCallback& callback,
    scoped_refptr<const Extension> extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  PolicyValueStore* store = GetStoreFor(extension->id());
  if (store) {
    callback.Run(store);
  } else {
    // First time that an extension calls storage.managed.get(). Create the
    // store and load it with the current policy, and don't send event
    // notifications.
    CreateStoreFor(
        extension->id(),
        false,
        base::Bind(&ManagedValueStoreCache::RunWithValueStoreForExtension,
                   base::Unretained(this),
                   callback,
                   extension));
  }
}

void ManagedValueStoreCache::DeleteStorageSoon(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  PolicyValueStore* store = GetStoreFor(extension_id);
  if (!store) {
    // It's possible that the store exists, but hasn't been loaded yet
    // (because the extension is unloaded, for example). Open the database to
    // clear it if it exists.
    // TODO(joaodasilva): move this check to a ValueStore method.
    if (base::DirectoryExists(base_path_.AppendASCII(extension_id))) {
      CreateStoreFor(
          extension_id,
          false,
          base::Bind(&ManagedValueStoreCache::DeleteStorageSoon,
                     base::Unretained(this),
                     extension_id));
    }
  } else {
    store->DeleteStorage();
    store_map_.erase(extension_id);
  }
}

void ManagedValueStoreCache::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                             const policy::PolicyMap& previous,
                                             const policy::PolicyMap& current) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  PolicyValueStore* store = GetStoreFor(extension_id);
  if (!store) {
    // The extension hasn't executed any storage.managed.* calls, and isn't
    // listening for onChanged() either. Ignore this notification in that case.
    return;
  }
  // Update the policy on the backing store, and fire notifications if it
  // changed.
  store->SetCurrentPolicy(*current_policy, true);
}

void ManagedValueStoreCache::OnListenerAdded(
    const EventListenerInfo& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(std::string(storage::OnChanged::kEventName), details.event_name);
  // This is invoked on several occasions:
  //
  // 1. when an extension first registers to observe storage.onChanged; in this
  //    case the backend doesn't have any previous data persisted, and it won't
  //    trigger a notification.
  //
  // 2. when the browser starts up and all existing extensions re-register for
  //    the onChanged event. In this case, if the current policy differs from
  //    the persisted version then a notification will be sent.
  //
  // 3. a policy update just occurred and sent a notification, and an extension
  //    with EventPages that is observing onChanged just woke up and registed
  //    again. In this case the policy update already persisted the current
  //    policy version, and |store| already exists.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ManagedValueStoreCache::CreateForExtensionOnFILE,
                 base::Unretained(this),
                 details.extension_id));
}

void ManagedValueStoreCache::CreateForExtensionOnFILE(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  PolicyValueStore* store = GetStoreFor(extension_id);
  if (!store)
    CreateStoreFor(extension_id, true, base::Closure());
}

PolicyValueStore* ManagedValueStoreCache::GetStoreFor(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  PolicyValueStoreMap::iterator it = store_map_.find(extension_id);
  if (it == store_map_.end())
    return NULL;
  return it->second.get();
}

void ManagedValueStoreCache::CreateStoreFor(
    const std::string& extension_id,
    bool notify_if_changed,
    const base::Closure& continuation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!GetStoreFor(extension_id));
  // Creating or loading an existing database requires an immediate update
  // with the current policy for the corresponding extension, which must be
  // retrieved on UI.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ManagedValueStoreCache::GetInitialPolicy,
                 weak_this_on_ui_,
                 extension_id,
                 notify_if_changed,
                 continuation));
}

void ManagedValueStoreCache::GetInitialPolicy(
    const std::string& extension_id,
    bool notify_if_changed,
    const base::Closure& continuation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  policy::PolicyService* policy_service = GetPolicyService();

  // If initialization of POLICY_DOMAIN_EXTENSIONS isn't complete then all the
  // policies served are empty; let the extension see what's cached in LevelDB
  // in that case. The PolicyService will issue notifications once new policies
  // are ready.
  scoped_ptr<policy::PolicyMap> policy;
  if (policy_service->IsInitializationComplete(
          policy::POLICY_DOMAIN_EXTENSIONS)) {
    policy::PolicyNamespace ns(policy::POLICY_DOMAIN_EXTENSIONS, extension_id);
    policy = policy_service->GetPolicies(ns).DeepCopy();
  }

  // Now post back to FILE to create the database.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ManagedValueStoreCache::CreateStoreWithInitialPolicy,
                 base::Unretained(this),
                 extension_id,
                 notify_if_changed,
                 base::Passed(&policy),
                 continuation));
}

void ManagedValueStoreCache::CreateStoreWithInitialPolicy(
    const std::string& extension_id,
    bool notify_if_changed,
    scoped_ptr<policy::PolicyMap> initial_policy,
    const base::Closure& continuation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // If a 2nd call to CreateStoreFor() is issued before the 1st gets to execute
  // its UI task, then the 2nd will enter this function but the store has
  // already been created. Check for that.
  PolicyValueStore* store = GetStoreFor(extension_id);

  if (!store) {
    // Create it now.

    // If the database doesn't exist yet then this is the initial install,
    // and no notifications should be issued in that case.
    // TODO(joaodasilva): move this check to a ValueStore method.
    if (!base::DirectoryExists(base_path_.AppendASCII(extension_id)))
      notify_if_changed = false;

    store = new PolicyValueStore(
        extension_id,
        observers_,
        make_scoped_ptr(storage_factory_->Create(base_path_, extension_id)));
    store_map_[extension_id] = make_linked_ptr(store);
  }

  // Send the latest policy to the store, if it's already available.
  if (initial_policy)
    store->SetCurrentPolicy(*initial_policy, notify_if_changed);

  // And finally resume from where this process started.
  if (!continuation.is_null())
    continuation.Run();
}

policy::PolicyService* ManagedValueStoreCache::GetPolicyService() {
  policy::ProfilePolicyConnector* connector =
      policy::ProfilePolicyConnectorFactory::GetForProfile(profile_);
  return connector->policy_service();
}

}  // namespace extensions
