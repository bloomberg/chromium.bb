// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/managed_value_store_cache.h"

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings/policy_value_store.h"
#include "chrome/browser/extensions/settings/settings_storage_factory.h"
#include "chrome/browser/value_store/value_store_change.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

ManagedValueStoreCache::ManagedValueStoreCache(
    policy::PolicyService* policy_service,
    EventRouter* event_router,
    const scoped_refptr<SettingsStorageFactory>& factory,
    const scoped_refptr<SettingsObserverList>& observers,
    const FilePath& profile_path)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      weak_this_on_ui_(weak_factory_.GetWeakPtr()),
      policy_service_(policy_service),
      event_router_(event_router),
      storage_factory_(factory),
      observers_(observers),
      base_path_(profile_path.AppendASCII(
          ExtensionService::kManagedSettingsDirectoryName)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // |event_router| can be NULL on unit_tests.
  if (event_router_)
    event_router_->RegisterObserver(this, event_names::kOnSettingsChanged);
  policy_service_->AddObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);
}

ManagedValueStoreCache::~ManagedValueStoreCache() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!event_router_);
  // Delete the PolicyValueStores on FILE.
  store_map_.clear();
}

void ManagedValueStoreCache::ShutdownOnUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  policy_service_->RemoveObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);
  policy_service_ = NULL;
  if (event_router_)
    event_router_->UnregisterObserver(this);
  event_router_ = NULL;
  weak_factory_.InvalidateWeakPtrs();
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
    if (file_util::DirectoryExists(base_path_.AppendASCII(extension_id))) {
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

void ManagedValueStoreCache::OnPolicyUpdated(policy::PolicyDomain domain,
                                             const std::string& component_id,
                                             const policy::PolicyMap& previous,
                                             const policy::PolicyMap& current) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ManagedValueStoreCache::UpdatePolicyOnFILE,
                 base::Unretained(this),
                 std::string(component_id),
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
  DCHECK_EQ(std::string(event_names::kOnSettingsChanged), details.event_name);
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
  const policy::PolicyMap& policy = policy_service_->GetPolicies(
      policy::POLICY_DOMAIN_EXTENSIONS, extension_id);
  // Now post back to FILE to create the database.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ManagedValueStoreCache::CreateStoreWithInitialPolicy,
                 base::Unretained(this),
                 extension_id,
                 notify_if_changed,
                 base::Passed(policy.DeepCopy()),
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
    if (!file_util::DirectoryExists(base_path_.AppendASCII(extension_id)))
      notify_if_changed = false;

    store = new PolicyValueStore(
        extension_id,
        observers_,
        make_scoped_ptr(storage_factory_->Create(base_path_, extension_id)));
    store_map_[extension_id] = make_linked_ptr(store);
  }

  // Send the latest policy to the store.
  store->SetCurrentPolicy(*initial_policy, notify_if_changed);

  // And finally resume from where this process started.
  if (!continuation.is_null())
    continuation.Run();
}

}  // namespace extensions
