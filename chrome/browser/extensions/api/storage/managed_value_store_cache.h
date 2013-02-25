// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_MANAGED_VALUE_STORE_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_MANAGED_VALUE_STORE_CACHE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/api/storage/settings_observer.h"
#include "chrome/browser/extensions/api/storage/value_store_cache.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/policy/policy_service.h"

class Profile;

namespace policy {
class PolicyMap;
}

namespace extensions {

class PolicyValueStore;
class SettingsStorageFactory;

// A ValueStoreCache that manages a PolicyValueStore for each extension that
// uses the storage.managed namespace. This class observes policy changes and
// which extensions listen for storage.onChanged(), and sends the appropriate
// updates to the corresponding PolicyValueStore on the FILE thread.
class ManagedValueStoreCache : public ValueStoreCache,
                               public policy::PolicyService::Observer,
                               public EventRouter::Observer {
 public:
  // |factory| is used to create databases for the PolicyValueStores.
  // |observers| is the list of SettingsObservers to notify when a ValueStore
  // changes.
  ManagedValueStoreCache(Profile* profile,
                         const scoped_refptr<SettingsStorageFactory>& factory,
                         const scoped_refptr<SettingsObserverList>& observers);
  virtual ~ManagedValueStoreCache();

 private:
  class ExtensionTracker;

  // Maps an extension ID to its PolicyValueStoreMap.
  typedef std::map<std::string, linked_ptr<PolicyValueStore> >
      PolicyValueStoreMap;

  // ValueStoreCache implementation:
  virtual void ShutdownOnUI() OVERRIDE;
  virtual void RunWithValueStoreForExtension(
      const StorageCallback& callback,
      scoped_refptr<const Extension> extension) OVERRIDE;
  virtual void DeleteStorageSoon(const std::string& extension_id) OVERRIDE;

  // PolicyService::Observer implementation:
  virtual void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                               const policy::PolicyMap& previous,
                               const policy::PolicyMap& current) OVERRIDE;

  // Posted by OnPolicyUpdated() to update a PolicyValueStore on the FILE
  // thread.
  void UpdatePolicyOnFILE(const std::string& extension_id,
                          scoped_ptr<policy::PolicyMap> current_policy);

  // EventRouter::Observer implementation:
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;

  // Posted by OnListenerAdded() to load or create a PolicyValueStore for the
  // given |extension_id|.
  void CreateForExtensionOnFILE(const std::string& extension_id);

  // Returns an existing PolicyValueStore for |extension_id|, or NULL.
  PolicyValueStore* GetStoreFor(const std::string& extension_id);

  // Creates a new PolicyValueStore for |extension_id|. This may open an
  // existing database, or create a new one. This also sends the current policy
  // for |extension_id| to the database. When |notify_if_changed| is true,
  // a notification is sent with the changes between the current policy and the
  // previously stored policy, if there are any.
  //
  // Since this is used on FILE but must retrieve the current policy, this
  // method first posts GetInitialPolicy() to UI and then resumes in
  // CreateStoreWithInitialPolicy(). If |continuation| is not null then it
  // will be invoked after the store is created.
  //
  // CreateStoreFor() can be safely invoked from any method on the FILE thread.
  // It posts to UI used |weak_this_on_ui_|, so that the task is dropped if
  // ShutdownOnUI() has been invoked. Otherwise, GetInitialPolicy() executes
  // on UI and can safely post CreateStoreWithInitialPolicy to FILE.
  // CreateStoreWithInitialPolicy then guarantees that a store for
  // |extension_id| exists or is created, and then executes the |continuation|;
  // so when the |continuation| executes, a store for |extension_id| is
  // guaranteed to exist.
  void CreateStoreFor(const std::string& extension_id,
                      bool notify_if_changed,
                      const base::Closure& continuation);

  // Helper for CreateStoreFor, invoked on UI.
  void GetInitialPolicy(const std::string& extension_id,
                        bool notify_if_changed,
                        const base::Closure& continuation);

  // Helper for CreateStoreFor, invoked on FILE.
  void CreateStoreWithInitialPolicy(const std::string& extension_id,
                                    bool notify_if_changed,
                                    scoped_ptr<policy::PolicyMap> policy,
                                    const base::Closure& continuation);

  // Used to create a WeakPtr valid on the UI thread, so that FILE tasks can
  // post back to UI.
  base::WeakPtrFactory<ManagedValueStoreCache> weak_factory_;

  // A WeakPtr to |this| that is valid on UI. This is used by tasks on the FILE
  // thread to post back to UI.
  base::WeakPtr<ManagedValueStoreCache> weak_this_on_ui_;

  // The profile that owns the extension system being used. This is used to
  // get the PolicyService, the EventRouter and the ExtensionService.
  Profile* profile_;

  // The EventRouter is created before the SettingsFrontend (which owns the
  // instance of this class), and the SettingsFrontend is also destroyed before
  // the EventRouter is. |event_router_| is thus valid for the lifetime of this
  // object, until ShutdownOnUI() is invoked. Lives on UI.
  EventRouter* event_router_;

  // Observes extension loading and unloading, and keeps the Profile's
  // PolicyService aware of the current list of extensions.
  scoped_ptr<ExtensionTracker> extension_tracker_;

  // These live on the FILE thread.
  scoped_refptr<SettingsStorageFactory> storage_factory_;
  scoped_refptr<SettingsObserverList> observers_;
  base::FilePath base_path_;

  // All the PolicyValueStores live on the FILE thread, and |store_map_| can be
  // accessed only on the FILE thread as well.
  PolicyValueStoreMap store_map_;

  DISALLOW_COPY_AND_ASSIGN(ManagedValueStoreCache);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_MANAGED_VALUE_STORE_CACHE_H_
