// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/settings_backend.h"

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/settings/failing_settings_storage.h"
#include "chrome/browser/extensions/settings/settings_storage_cache.h"
#include "chrome/browser/extensions/settings/settings_storage_factory.h"
#include "chrome/browser/extensions/settings/settings_storage_quota_enforcer.h"
#include "chrome/browser/extensions/settings/settings_sync_util.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

namespace {

// Total quota for all settings per extension, in bytes.  100K should be enough
// for most uses, but this can be increased as demand increases.
const size_t kTotalQuotaBytes = 100 * 1024;

// Quota for each setting.  Sync supports 5k per setting, so be a bit more
// restrictive than that.
const size_t kQuotaPerSettingBytes = 2048;

// Max number of settings per extension.  Keep low for sync.
const size_t kMaxSettingKeys = 512;

}  // namespace

SettingsBackend::SettingsBackend(
    const scoped_refptr<SettingsStorageFactory>& storage_factory,
    const FilePath& base_path,
    const scoped_refptr<SettingsObserverList>& observers)
    : storage_factory_(storage_factory),
      base_path_(base_path),
      observers_(observers),
      sync_type_(syncable::UNSPECIFIED),
      sync_processor_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

SettingsBackend::~SettingsBackend() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

SettingsStorage* SettingsBackend::GetStorage(
    const std::string& extension_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DictionaryValue empty;
  return GetOrCreateStorageWithSyncData(extension_id, empty);
}

SyncableSettingsStorage* SettingsBackend::GetOrCreateStorageWithSyncData(
    const std::string& extension_id, const DictionaryValue& sync_data) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  StorageObjMap::iterator maybe_storage = storage_objs_.find(extension_id);
  if (maybe_storage != storage_objs_.end()) {
    return maybe_storage->second.get();
  }

  SettingsStorage* storage = storage_factory_->Create(base_path_, extension_id);
  if (storage) {
    storage = new SettingsStorageCache(storage);
    // It's fine to create the quota enforcer underneath the sync layer, since
    // sync will only go ahead if each underlying storage operation succeeds.
    storage = new SettingsStorageQuotaEnforcer(
        kTotalQuotaBytes, kQuotaPerSettingBytes, kMaxSettingKeys, storage);
  } else {
    storage = new FailingSettingsStorage();
  }

  linked_ptr<SyncableSettingsStorage> syncable_storage(
      new SyncableSettingsStorage(
          observers_,
          extension_id,
          storage));
  storage_objs_[extension_id] = syncable_storage;

  if (sync_processor_) {
    SyncError error =
        syncable_storage->StartSyncing(sync_type_, sync_data, sync_processor_);
    if (error.IsSet()) {
      DisableSyncForExtension(extension_id);
    }
  }

  return syncable_storage.get();
}

void SettingsBackend::DeleteStorage(const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  StorageObjMap::iterator maybe_storage = storage_objs_.find(extension_id);
  if (maybe_storage == storage_objs_.end()) {
    return;
  }

  // Clear settings when the extension is uninstalled.  Leveldb implementations
  // will also delete the database from disk when the object is destroyed as
  // a result of being removed from |storage_objs_|.
  maybe_storage->second->Clear();
  storage_objs_.erase(maybe_storage);
}

std::set<std::string> SettingsBackend::GetKnownExtensionIDs() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::set<std::string> result;

  // Extension IDs live in-memory and/or on disk.  The cache will contain all
  // that are in-memory.
  for (StorageObjMap::iterator it = storage_objs_.begin();
      it != storage_objs_.end(); ++it) {
    result.insert(it->first);
  }

  // Leveldb databases are directories inside base_path_.
  file_util::FileEnumerator::FindInfo find_info;
  file_util::FileEnumerator extension_dirs(
      base_path_, false, file_util::FileEnumerator::DIRECTORIES);
  while (!extension_dirs.Next().empty()) {
    extension_dirs.GetFindInfo(&find_info);
    FilePath extension_dir(file_util::FileEnumerator::GetFilename(find_info));
    DCHECK(!extension_dir.IsAbsolute());
    // Extension IDs are created as std::strings so they *should* be ASCII.
    std::string maybe_as_ascii(extension_dir.MaybeAsASCII());
    if (!maybe_as_ascii.empty()) {
      result.insert(maybe_as_ascii);
    }
  }

  return result;
}

void SettingsBackend::DisableSyncForExtension(
    const std::string& extension_id) const {
  linked_ptr<SyncableSettingsStorage> storage = storage_objs_[extension_id];
  DCHECK(storage.get());
  storage->StopSyncing();
}

static void AddAllSyncData(
    const std::string& extension_id,
    const DictionaryValue& src,
    SyncDataList* dst) {
  for (DictionaryValue::Iterator it(src); it.HasNext(); it.Advance()) {
    dst->push_back(
        settings_sync_util::CreateData(extension_id, it.key(), it.value()));
  }
}

SyncDataList SettingsBackend::GetAllSyncData(
    syncable::ModelType type) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // Ignore the type, it's just for sanity checking; assume that whatever base
  // path we're constructed with is correct for the sync type.
  DCHECK(type == syncable::EXTENSION_SETTINGS ||
         type == syncable::APP_SETTINGS);

  // For all extensions, get all their settings.  This has the effect
  // of bringing in the entire state of extension settings in memory; sad.
  SyncDataList all_sync_data;
  std::set<std::string> known_extension_ids(GetKnownExtensionIDs());

  for (std::set<std::string>::const_iterator it = known_extension_ids.begin();
      it != known_extension_ids.end(); ++it) {
    SettingsStorage::ReadResult maybe_settings = GetStorage(*it)->Get();
    if (maybe_settings.HasError()) {
      LOG(WARNING) << "Failed to get settings for " << *it << ": " <<
          maybe_settings.error();
      continue;
    }
    AddAllSyncData(*it, maybe_settings.settings(), &all_sync_data);
  }

  return all_sync_data;
}

SyncError SettingsBackend::MergeDataAndStartSyncing(
    syncable::ModelType type,
    const SyncDataList& initial_sync_data,
    SyncChangeProcessor* sync_processor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(type == syncable::EXTENSION_SETTINGS ||
         type == syncable::APP_SETTINGS);
  DCHECK_EQ(sync_type_, syncable::UNSPECIFIED);
  DCHECK(!sync_processor_);

  sync_type_ = type;
  sync_processor_ = sync_processor;

  // Group the initial sync data by extension id.
  std::map<std::string, linked_ptr<DictionaryValue> > grouped_sync_data;
  for (SyncDataList::const_iterator it = initial_sync_data.begin();
      it != initial_sync_data.end(); ++it) {
    SettingSyncData data(*it);
    linked_ptr<DictionaryValue> sync_data =
        grouped_sync_data[data.extension_id()];
    if (!sync_data.get()) {
      sync_data = linked_ptr<DictionaryValue>(new DictionaryValue());
      grouped_sync_data[data.extension_id()] = sync_data;
    }
    DCHECK(!sync_data->HasKey(data.key())) <<
        "Duplicate settings for " << data.extension_id() << "/" << data.key();
    sync_data->Set(data.key(), data.value().DeepCopy());
  }

  // Start syncing all existing storage areas.  Any storage areas created in
  // the future will start being synced as part of the creation process.
  for (StorageObjMap::iterator it = storage_objs_.begin();
      it != storage_objs_.end(); ++it) {
    std::map<std::string, linked_ptr<DictionaryValue> >::iterator
        maybe_sync_data = grouped_sync_data.find(it->first);
    SyncError error;
    if (maybe_sync_data != grouped_sync_data.end()) {
      error = it->second->StartSyncing(
          type, *maybe_sync_data->second, sync_processor);
      grouped_sync_data.erase(it->first);
    } else {
      DictionaryValue empty;
      error = it->second->StartSyncing(type, empty, sync_processor);
    }
    if (error.IsSet()) {
      DisableSyncForExtension(it->first);
    }
  }

  // Eagerly create and init the rest of the storage areas that have sync data.
  // Under normal circumstances (i.e. not first-time sync) this will be all of
  // them.
  for (std::map<std::string, linked_ptr<DictionaryValue> >::iterator it =
      grouped_sync_data.begin(); it != grouped_sync_data.end(); ++it) {
    GetOrCreateStorageWithSyncData(it->first, *it->second);
  }

  return SyncError();
}

SyncError SettingsBackend::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& sync_changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(sync_processor_);

  // Group changes by extension, to pass all changes in a single method call.
  std::map<std::string, SettingSyncDataList> grouped_sync_data;
  for (SyncChangeList::const_iterator it = sync_changes.begin();
      it != sync_changes.end(); ++it) {
    SettingSyncData data(*it);
    grouped_sync_data[data.extension_id()].push_back(data);
  }

  // Create any storage areas that don't exist yet but have sync data.
  DictionaryValue empty;
  for (std::map<std::string, SettingSyncDataList>::iterator
      it = grouped_sync_data.begin(); it != grouped_sync_data.end(); ++it) {
    SyncError error =
        GetOrCreateStorageWithSyncData(it->first, empty)->
            ProcessSyncChanges(it->second);
    if (error.IsSet()) {
      DisableSyncForExtension(it->first);
    }
  }

  return SyncError();
}

void SettingsBackend::StopSyncing(syncable::ModelType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(type == syncable::EXTENSION_SETTINGS ||
         type == syncable::APP_SETTINGS);
  DCHECK_EQ(type, sync_type_);
  DCHECK(sync_processor_);

  sync_type_ = syncable::UNSPECIFIED;
  sync_processor_ = NULL;

  for (StorageObjMap::iterator it = storage_objs_.begin();
      it != storage_objs_.end(); ++it) {
    // Some storage areas may have already stopped syncing if they had areas
    // and syncing was disabled, but StopSyncing is safe to call multiple times.
    it->second->StopSyncing();
  }
}

}  // namespace extensions
