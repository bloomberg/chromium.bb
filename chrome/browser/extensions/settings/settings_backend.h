// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_BACKEND_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_BACKEND_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "base/task.h"
#include "chrome/browser/extensions/settings/settings_leveldb_storage.h"
#include "chrome/browser/extensions/settings/settings_observer.h"
#include "chrome/browser/extensions/settings/syncable_settings_storage.h"
#include "chrome/browser/sync/api/syncable_service.h"

namespace extensions {

// Manages SettingsStorage objects for extensions, including routing
// changes from sync to them.
// Lives entirely on the FILE thread.
class SettingsBackend : public SyncableService {
 public:
  // |storage_factory| is use to create leveldb storage areas.
  // |base_path| is the base of the extension settings directory, so the
  // databases will be at base_path/extension_id.
  // |observers| is the list of observers to settings changes.
  explicit SettingsBackend(
      // Ownership NOT taken.
      SettingsStorageFactory* storage_factory,
      const FilePath& base_path,
      const scoped_refptr<SettingsObserverList>& observers);

  virtual ~SettingsBackend();

  // Gets a weak reference to the storage area for a given extension.
  // Must be run on the FILE thread.
  SettingsStorage* GetStorage(const std::string& extension_id) const;

  // Deletes all setting data for an extension.  Call on the FILE thread.
  void DeleteStorage(const std::string& extension_id);

  // SyncableService implementation.
  virtual SyncDataList GetAllSyncData(syncable::ModelType type) const OVERRIDE;
  virtual SyncError MergeDataAndStartSyncing(
      syncable::ModelType type,
      const SyncDataList& initial_sync_data,
      SyncChangeProcessor* sync_processor) OVERRIDE;
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE;
  virtual void StopSyncing(syncable::ModelType type) OVERRIDE;

 private:
  // Gets a weak reference to the storage area for a given extension,
  // initializing sync with some initial data if sync enabled.
  //
  // By default this will be of a cached LEVELDB storage, but on failure to
  // create a leveldb instance will fall back to cached NOOP storage.
  SyncableSettingsStorage* GetOrCreateStorageWithSyncData(
      const std::string& extension_id, const DictionaryValue& sync_data) const;

  // Gets all extension IDs known to extension settings.  This may not be all
  // installed extensions.
  std::set<std::string> GetKnownExtensionIDs() const;

  // Disable the syncing of the storage area for |extension_id|.
  void DisableSyncForExtension(const std::string& extension_id) const;

  // The Factory to use for creating leveldb storage areas.  Not owned.
  SettingsStorageFactory* const storage_factory_;

  // The base file path to create any leveldb databases at.
  const FilePath base_path_;

  // The list of observers to settings changes.
  const scoped_refptr<SettingsObserverList> observers_;

  // A cache of SettingsStorage objects that have already been created.
  // Ensure that there is only ever one created per extension.
  typedef std::map<std::string, linked_ptr<SyncableSettingsStorage> >
      StorageObjMap;
  mutable StorageObjMap storage_objs_;

  // Current sync model type.  Will be UNSPECIFIED if sync hasn't been enabled
  // yet, and either SETTINGS or APP_SETTINGS if it has been.
  syncable::ModelType sync_type_;

  // Current sync processor, if any.
  SyncChangeProcessor* sync_processor_;

  DISALLOW_COPY_AND_ASSIGN(SettingsBackend);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_BACKEND_H_
