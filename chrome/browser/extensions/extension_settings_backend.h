// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_BACKEND_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_BACKEND_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/extensions/syncable_extension_settings_storage.h"
#include "chrome/browser/sync/api/syncable_service.h"

// Manages ExtensionSettingsStorage objects for extensions, including routing
// changes from sync to them.
// Lives entirely on the FILE thread.
class ExtensionSettingsBackend : public SyncableService {
 public:
  // File path is the base of the extension settings directory.
  // The databases will be at base_path/extension_id.
  explicit ExtensionSettingsBackend(const FilePath& base_path);

  virtual ~ExtensionSettingsBackend();

  // Gets a weak reference to the storage area for a given extension.
  // Must be run on the FILE thread.
  //
  // By default this will be an ExtensionSettingsLeveldbStorage, but on
  // failure to create a leveldb instance will fall back to
  // InMemoryExtensionSettingsStorage.
  ExtensionSettingsStorage* GetStorage(
      const std::string& extension_id) const;

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
  SyncableExtensionSettingsStorage* GetOrCreateStorageWithSyncData(
      const std::string& extension_id, const DictionaryValue& sync_data) const;

  // Gets all extension IDs known to extension settings.  This may not be all
  // installed extensions.
  std::set<std::string> GetKnownExtensionIDs() const;

  // The base file path to create any leveldb databases at.
  const FilePath base_path_;

  // A cache of ExtensionSettingsStorage objects that have already been created.
  // Ensure that there is only ever one created per extension.
  typedef std::map<std::string, linked_ptr<SyncableExtensionSettingsStorage> >
      StorageObjMap;
  mutable StorageObjMap storage_objs_;

  // Current sync processor, if any.
  SyncChangeProcessor* sync_processor_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsBackend);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_BACKEND_H_
