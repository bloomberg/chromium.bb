// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_STORAGE_BACKEND_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_STORAGE_BACKEND_H_

#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/api/storage/settings_storage_factory.h"
#include "extensions/browser/api/storage/settings_storage_quota_enforcer.h"
#include "sync/api/syncable_service.h"

namespace syncer {
class SyncErrorFactory;
}

namespace extensions {

class SettingsSyncProcessor;
class SyncableSettingsStorage;

// Manages ValueStore objects for extensions, including routing
// changes from sync to them.
// Lives entirely on the FILE thread.
class SyncStorageBackend : public syncer::SyncableService {
 public:
  // |storage_factory| is use to create leveldb storage areas.
  // |base_path| is the base of the extension settings directory, so the
  // databases will be at base_path/extension_id.
  // |observers| is the list of observers to settings changes.
  SyncStorageBackend(
      const scoped_refptr<SettingsStorageFactory>& storage_factory,
      const base::FilePath& base_path,
      const SettingsStorageQuotaEnforcer::Limits& quota,
      const scoped_refptr<SettingsObserverList>& observers,
      syncer::ModelType sync_type,
      const syncer::SyncableService::StartSyncFlare& flare);

  virtual ~SyncStorageBackend();

  virtual ValueStore* GetStorage(const std::string& extension_id);
  virtual void DeleteStorage(const std::string& extension_id);

  // syncer::SyncableService implementation.
  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type)
      const OVERRIDE;
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;

 private:
  // Gets a weak reference to the storage area for a given extension,
  // initializing sync with some initial data if sync enabled.
  SyncableSettingsStorage* GetOrCreateStorageWithSyncData(
      const std::string& extension_id,
      const base::DictionaryValue& sync_data) const;

  // Gets all extension IDs known to extension settings.  This may not be all
  // installed extensions.
  std::set<std::string> GetKnownExtensionIDs() const;

  // Creates a new SettingsSyncProcessor for an extension.
  scoped_ptr<SettingsSyncProcessor> CreateSettingsSyncProcessor(
      const std::string& extension_id) const;

  // The Factory to use for creating new ValueStores.
  const scoped_refptr<SettingsStorageFactory> storage_factory_;

  // The base file path to use when creating new ValueStores.
  const base::FilePath base_path_;

  // Quota limits (see SettingsStorageQuotaEnforcer).
  const SettingsStorageQuotaEnforcer::Limits quota_;

  // The list of observers to settings changes.
  const scoped_refptr<SettingsObserverList> observers_;

  // A cache of ValueStore objects that have already been created.
  // Ensure that there is only ever one created per extension.
  typedef std::map<std::string, linked_ptr<SyncableSettingsStorage> >
      StorageObjMap;
  mutable StorageObjMap storage_objs_;

  // Current sync model type. Either EXTENSION_SETTINGS or APP_SETTINGS.
  syncer::ModelType sync_type_;

  // Current sync processor, if any.
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // Current sync error handler if any.
  scoped_ptr<syncer::SyncErrorFactory> sync_error_factory_;

  syncer::SyncableService::StartSyncFlare flare_;

  DISALLOW_COPY_AND_ASSIGN(SyncStorageBackend);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_STORAGE_BACKEND_H_
