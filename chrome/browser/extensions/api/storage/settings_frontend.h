// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_FRONTEND_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_FRONTEND_H_

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/storage/settings_observer.h"
#include "chrome/browser/extensions/api/storage/settings_storage_factory.h"
#include "chrome/browser/extensions/api/storage/settings_storage_quota_enforcer.h"
#include "chrome/browser/extensions/api/storage/value_store_cache.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "sync/api/syncable_service.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// The component of extension settings which runs on the UI thread, as opposed
// to SettingsBackend which lives on the FILE thread.
// All public methods, must be called on the UI thread, with the exception of
// GetBackendForSync(), which must be called on the FILE thread.
class SettingsFrontend : public BrowserContextKeyedAPI {
 public:
  // Returns the current instance for |context|.
  static SettingsFrontend* Get(content::BrowserContext* context);

  // Creates with a specific |storage_factory|. Caller owns the object.
  static SettingsFrontend* CreateForTesting(
      const scoped_refptr<SettingsStorageFactory>& storage_factory,
      content::BrowserContext* context);

  // Public so tests can create and delete their own instances.
  virtual ~SettingsFrontend();

  // Must only be called from the FILE thread. |type| should be either
  // APP_SETTINGS or EXTENSION_SETTINGS.
  syncer::SyncableService* GetBackendForSync(syncer::ModelType type) const;

  // Returns true if |settings_namespace| is a valid namespace.
  bool IsStorageEnabled(settings_namespace::Namespace settings_namespace) const;

  // Runs |callback| with the storage area of the given |settings_namespace|
  // for the |extension|.
  void RunWithStorage(scoped_refptr<const Extension> extension,
                      settings_namespace::Namespace settings_namespace,
                      const ValueStoreCache::StorageCallback& callback);

  // Deletes the settings for the given |extension_id|.
  void DeleteStorageSoon(const std::string& extension_id);

  // Gets the thread-safe observer list.
  scoped_refptr<SettingsObserverList> GetObservers();

  void DisableStorageForTesting(
      settings_namespace::Namespace settings_namespace);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SettingsFrontend>* GetFactoryInstance();
  static const char* service_name();
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

 private:
  friend class BrowserContextKeyedAPIFactory<SettingsFrontend>;

  typedef std::map<settings_namespace::Namespace, ValueStoreCache*> CacheMap;

  // Constructor for normal BrowserContextKeyedAPI usage.
  explicit SettingsFrontend(content::BrowserContext* context);

  // Constructor for tests.
  SettingsFrontend(const scoped_refptr<SettingsStorageFactory>& storage_factory,
                   content::BrowserContext* context);

  void Init(const scoped_refptr<SettingsStorageFactory>& storage_factory);

  // The quota limit configurations for the local and sync areas, taken out of
  // the schema in chrome/common/extensions/api/storage.json.
  const SettingsStorageQuotaEnforcer::Limits local_quota_limit_;
  const SettingsStorageQuotaEnforcer::Limits sync_quota_limit_;

  // The (non-incognito) browser context this Frontend belongs to.
  content::BrowserContext* const browser_context_;

  // List of observers to settings changes.
  scoped_refptr<SettingsObserverList> observers_;

  // Observer for |browser_context_|.
  scoped_ptr<SettingsObserver> browser_context_observer_;

  // Maps a known namespace to its corresponding ValueStoreCache. The caches
  // are owned by this object.
  CacheMap caches_;

  DISALLOW_COPY_AND_ASSIGN(SettingsFrontend);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_FRONTEND_H_
