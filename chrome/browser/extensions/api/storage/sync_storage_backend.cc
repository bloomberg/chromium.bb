// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/sync_storage_backend.h"

#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "chrome/browser/extensions/api/storage/settings_sync_processor.h"
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/api/storage/syncable_settings_storage.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_error_factory.h"

using content::BrowserThread;

namespace extensions {

namespace {

void AddAllSyncData(const std::string& extension_id,
                    const base::DictionaryValue& src,
                    syncer::ModelType type,
                    syncer::SyncDataList* dst) {
  for (base::DictionaryValue::Iterator it(src); !it.IsAtEnd(); it.Advance()) {
    dst->push_back(settings_sync_util::CreateData(
        extension_id, it.key(), it.value(), type));
  }
}

}  // namespace

SyncStorageBackend::SyncStorageBackend(
    const scoped_refptr<SettingsStorageFactory>& storage_factory,
    const base::FilePath& base_path,
    const SettingsStorageQuotaEnforcer::Limits& quota,
    const scoped_refptr<SettingsObserverList>& observers,
    syncer::ModelType sync_type,
    const syncer::SyncableService::StartSyncFlare& flare)
    : storage_factory_(storage_factory),
      base_path_(base_path),
      quota_(quota),
      observers_(observers),
      sync_type_(sync_type),
      flare_(flare) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(sync_type_ == syncer::EXTENSION_SETTINGS ||
         sync_type_ == syncer::APP_SETTINGS);
}

SyncStorageBackend::~SyncStorageBackend() {}

ValueStore* SyncStorageBackend::GetStorage(const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::DictionaryValue empty;
  return GetOrCreateStorageWithSyncData(extension_id, empty);
}

SyncableSettingsStorage* SyncStorageBackend::GetOrCreateStorageWithSyncData(
    const std::string& extension_id,
    const base::DictionaryValue& sync_data) const {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  StorageObjMap::iterator maybe_storage = storage_objs_.find(extension_id);
  if (maybe_storage != storage_objs_.end()) {
    return maybe_storage->second.get();
  }

  scoped_ptr<SettingsStorageQuotaEnforcer> storage(
      new SettingsStorageQuotaEnforcer(
          quota_, storage_factory_->Create(base_path_, extension_id)));

  // It's fine to create the quota enforcer underneath the sync layer, since
  // sync will only go ahead if each underlying storage operation succeeds.
  linked_ptr<SyncableSettingsStorage> syncable_storage(
      new SyncableSettingsStorage(
          observers_, extension_id, storage.release(), sync_type_, flare_));
  storage_objs_[extension_id] = syncable_storage;

  if (sync_processor_.get()) {
    syncer::SyncError error = syncable_storage->StartSyncing(
        sync_data, CreateSettingsSyncProcessor(extension_id).Pass());
    if (error.IsSet())
      syncable_storage.get()->StopSyncing();
  }
  return syncable_storage.get();
}

void SyncStorageBackend::DeleteStorage(const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  // Clear settings when the extension is uninstalled.  Leveldb implementations
  // will also delete the database from disk when the object is destroyed as a
  // result of being removed from |storage_objs_|.
  //
  // TODO(kalman): always GetStorage here (rather than only clearing if it
  // exists) since the storage area may have been unloaded, but we still want
  // to clear the data from disk.
  // However, this triggers http://crbug.com/111072.
  StorageObjMap::iterator maybe_storage = storage_objs_.find(extension_id);
  if (maybe_storage == storage_objs_.end())
    return;
  maybe_storage->second->Clear();
  storage_objs_.erase(extension_id);
}

std::set<std::string> SyncStorageBackend::GetKnownExtensionIDs() const {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  std::set<std::string> result;

  // Storage areas can be in-memory as well as on disk. |storage_objs_| will
  // contain all that are in-memory.
  for (StorageObjMap::iterator it = storage_objs_.begin();
       it != storage_objs_.end();
       ++it) {
    result.insert(it->first);
  }

  // Leveldb databases are directories inside |base_path_|.
  base::FileEnumerator extension_dirs(
      base_path_, false, base::FileEnumerator::DIRECTORIES);
  while (!extension_dirs.Next().empty()) {
    base::FilePath extension_dir = extension_dirs.GetInfo().GetName();
    DCHECK(!extension_dir.IsAbsolute());
    // Extension IDs are created as std::strings so they *should* be ASCII.
    std::string maybe_as_ascii(extension_dir.MaybeAsASCII());
    if (!maybe_as_ascii.empty()) {
      result.insert(maybe_as_ascii);
    }
  }

  return result;
}

syncer::SyncDataList SyncStorageBackend::GetAllSyncData(syncer::ModelType type)
    const {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  // Ignore the type, it's just for sanity checking; assume that whatever base
  // path we're constructed with is correct for the sync type.
  DCHECK(type == syncer::EXTENSION_SETTINGS || type == syncer::APP_SETTINGS);

  // For all extensions, get all their settings.  This has the effect
  // of bringing in the entire state of extension settings in memory; sad.
  syncer::SyncDataList all_sync_data;
  std::set<std::string> known_extension_ids(GetKnownExtensionIDs());

  for (std::set<std::string>::const_iterator it = known_extension_ids.begin();
       it != known_extension_ids.end();
       ++it) {
    ValueStore::ReadResult maybe_settings =
        GetOrCreateStorageWithSyncData(*it, base::DictionaryValue())->Get();
    if (maybe_settings->HasError()) {
      LOG(WARNING) << "Failed to get settings for " << *it << ": "
                   << maybe_settings->error().message;
      continue;
    }
    AddAllSyncData(*it, maybe_settings->settings(), type, &all_sync_data);
  }

  return all_sync_data;
}

syncer::SyncMergeResult SyncStorageBackend::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK_EQ(sync_type_, type);
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK(sync_error_factory.get());

  sync_processor_ = sync_processor.Pass();
  sync_error_factory_ = sync_error_factory.Pass();

  // Group the initial sync data by extension id.
  std::map<std::string, linked_ptr<base::DictionaryValue> > grouped_sync_data;
  for (syncer::SyncDataList::const_iterator it = initial_sync_data.begin();
       it != initial_sync_data.end();
       ++it) {
    SettingSyncData data(*it);
    linked_ptr<base::DictionaryValue> sync_data =
        grouped_sync_data[data.extension_id()];
    if (!sync_data.get()) {
      sync_data =
          linked_ptr<base::DictionaryValue>(new base::DictionaryValue());
      grouped_sync_data[data.extension_id()] = sync_data;
    }
    DCHECK(!sync_data->HasKey(data.key())) << "Duplicate settings for "
                                           << data.extension_id() << "/"
                                           << data.key();
    sync_data->SetWithoutPathExpansion(data.key(), data.value().DeepCopy());
  }

  // Start syncing all existing storage areas.  Any storage areas created in
  // the future will start being synced as part of the creation process.
  for (StorageObjMap::iterator it = storage_objs_.begin();
       it != storage_objs_.end();
       ++it) {
    std::map<std::string, linked_ptr<base::DictionaryValue> >::iterator
        maybe_sync_data = grouped_sync_data.find(it->first);
    syncer::SyncError error;
    if (maybe_sync_data != grouped_sync_data.end()) {
      error = it->second->StartSyncing(
          *maybe_sync_data->second,
          CreateSettingsSyncProcessor(it->first).Pass());
      grouped_sync_data.erase(it->first);
    } else {
      base::DictionaryValue empty;
      error = it->second->StartSyncing(
          empty, CreateSettingsSyncProcessor(it->first).Pass());
    }
    if (error.IsSet())
      it->second->StopSyncing();
  }

  // Eagerly create and init the rest of the storage areas that have sync data.
  // Under normal circumstances (i.e. not first-time sync) this will be all of
  // them.
  for (std::map<std::string, linked_ptr<base::DictionaryValue> >::iterator it =
           grouped_sync_data.begin();
       it != grouped_sync_data.end();
       ++it) {
    GetOrCreateStorageWithSyncData(it->first, *it->second);
  }

  return syncer::SyncMergeResult(type);
}

syncer::SyncError SyncStorageBackend::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& sync_changes) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(sync_processor_.get());

  // Group changes by extension, to pass all changes in a single method call.
  std::map<std::string, SettingSyncDataList> grouped_sync_data;
  for (syncer::SyncChangeList::const_iterator it = sync_changes.begin();
       it != sync_changes.end();
       ++it) {
    SettingSyncData data(*it);
    grouped_sync_data[data.extension_id()].push_back(data);
  }

  // Create any storage areas that don't exist yet but have sync data.
  base::DictionaryValue empty;
  for (std::map<std::string, SettingSyncDataList>::iterator it =
           grouped_sync_data.begin();
       it != grouped_sync_data.end();
       ++it) {
    SyncableSettingsStorage* storage =
        GetOrCreateStorageWithSyncData(it->first, empty);
    syncer::SyncError error = storage->ProcessSyncChanges(it->second);
    if (error.IsSet())
      storage->StopSyncing();
  }

  return syncer::SyncError();
}

void SyncStorageBackend::StopSyncing(syncer::ModelType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(type == syncer::EXTENSION_SETTINGS || type == syncer::APP_SETTINGS);
  DCHECK_EQ(sync_type_, type);

  for (StorageObjMap::iterator it = storage_objs_.begin();
       it != storage_objs_.end();
       ++it) {
    // Some storage areas may have already stopped syncing if they had areas
    // and syncing was disabled, but StopSyncing is safe to call multiple times.
    it->second->StopSyncing();
  }

  sync_processor_.reset();
  sync_error_factory_.reset();
}

scoped_ptr<SettingsSyncProcessor>
SyncStorageBackend::CreateSettingsSyncProcessor(const std::string& extension_id)
    const {
  CHECK(sync_processor_.get());
  return scoped_ptr<SettingsSyncProcessor>(new SettingsSyncProcessor(
      extension_id, sync_type_, sync_processor_.get()));
}

}  // namespace extensions
