// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/syncable_settings_storage.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/storage/settings_sync_processor.h"
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "sync/api/sync_data.h"
#include "sync/protocol/extension_setting_specifics.pb.h"

namespace extensions {

using content::BrowserThread;

SyncableSettingsStorage::SyncableSettingsStorage(
    const scoped_refptr<ObserverListThreadSafe<SettingsObserver> >&
        observers,
    const std::string& extension_id,
    ValueStore* delegate,
    syncer::ModelType sync_type,
    const syncer::SyncableService::StartSyncFlare& flare)
    : observers_(observers),
      extension_id_(extension_id),
      delegate_(delegate),
      sync_type_(sync_type),
      flare_(flare) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
}

SyncableSettingsStorage::~SyncableSettingsStorage() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
}

size_t SyncableSettingsStorage::GetBytesInUse(const std::string& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  return delegate_->GetBytesInUse(key);
}

size_t SyncableSettingsStorage::GetBytesInUse(
    const std::vector<std::string>& keys) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  return delegate_->GetBytesInUse(keys);
}

size_t SyncableSettingsStorage::GetBytesInUse() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  return delegate_->GetBytesInUse();
}

ValueStore::ReadResult SyncableSettingsStorage::Get(
    const std::string& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  return delegate_->Get(key);
}

ValueStore::ReadResult SyncableSettingsStorage::Get(
    const std::vector<std::string>& keys) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  return delegate_->Get(keys);
}

ValueStore::ReadResult SyncableSettingsStorage::Get() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  return delegate_->Get();
}

ValueStore::WriteResult SyncableSettingsStorage::Set(
    WriteOptions options, const std::string& key, const base::Value& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  WriteResult result = delegate_->Set(options, key, value);
  if (result->HasError()) {
    return result.Pass();
  }
  SyncResultIfEnabled(result);
  return result.Pass();
}

ValueStore::WriteResult SyncableSettingsStorage::Set(
    WriteOptions options, const base::DictionaryValue& values) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  WriteResult result = delegate_->Set(options, values);
  if (result->HasError()) {
    return result.Pass();
  }
  SyncResultIfEnabled(result);
  return result.Pass();
}

ValueStore::WriteResult SyncableSettingsStorage::Remove(
    const std::string& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  WriteResult result = delegate_->Remove(key);
  if (result->HasError()) {
    return result.Pass();
  }
  SyncResultIfEnabled(result);
  return result.Pass();
}

ValueStore::WriteResult SyncableSettingsStorage::Remove(
    const std::vector<std::string>& keys) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  WriteResult result = delegate_->Remove(keys);
  if (result->HasError()) {
    return result.Pass();
  }
  SyncResultIfEnabled(result);
  return result.Pass();
}

ValueStore::WriteResult SyncableSettingsStorage::Clear() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  WriteResult result = delegate_->Clear();
  if (result->HasError()) {
    return result.Pass();
  }
  SyncResultIfEnabled(result);
  return result.Pass();
}

bool SyncableSettingsStorage::Restore() {
  // If we're syncing, stop - we don't want to push the deletion of any data.
  // At next startup, when we start up the sync service, we'll get back any
  // data which was stored intact on Sync.
  // TODO (rdevlin.cronin): Investigate if there's a way we can trigger
  // MergeDataAndStartSyncing() to immediately get back any data we can,
  // and continue syncing.
  StopSyncing();
  return delegate_->Restore();
}

bool SyncableSettingsStorage::RestoreKey(const std::string& key) {
  // If we're syncing, stop - we don't want to push the deletion of any data.
  // At next startup, when we start up the sync service, we'll get back any
  // data which was stored intact on Sync.
  // TODO (rdevlin.cronin): Investigate if there's a way we can trigger
  // MergeDataAndStartSyncing() to immediately get back any data we can,
  // and continue syncing.
  StopSyncing();
  return delegate_->RestoreKey(key);
}

void SyncableSettingsStorage::SyncResultIfEnabled(
    const ValueStore::WriteResult& result) {
  if (result->changes().empty())
    return;

  if (sync_processor_.get()) {
    syncer::SyncError error = sync_processor_->SendChanges(result->changes());
    if (error.IsSet())
      StopSyncing();
  } else {
    // Tell sync to try and start soon, because syncable changes to sync_type_
    // have started happening. This will cause sync to call us back
    // asynchronously via StartSyncing(...) as soon as possible.
    flare_.Run(sync_type_);
  }
}

// Sync-related methods.

syncer::SyncError SyncableSettingsStorage::StartSyncing(
    const base::DictionaryValue& sync_state,
    scoped_ptr<SettingsSyncProcessor> sync_processor) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(!sync_processor_.get());

  sync_processor_ = sync_processor.Pass();
  sync_processor_->Init(sync_state);

  ReadResult maybe_settings = delegate_->Get();
  if (maybe_settings->HasError()) {
    return syncer::SyncError(
        FROM_HERE,
        syncer::SyncError::DATATYPE_ERROR,
        base::StringPrintf("Failed to get settings: %s",
            maybe_settings->error().message.c_str()),
        sync_processor_->type());
  }

  const base::DictionaryValue& settings = maybe_settings->settings();
  return sync_state.empty() ?
      SendLocalSettingsToSync(settings) :
      OverwriteLocalSettingsWithSync(sync_state, settings);
}

syncer::SyncError SyncableSettingsStorage::SendLocalSettingsToSync(
    const base::DictionaryValue& settings) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  ValueStoreChangeList changes;
  for (base::DictionaryValue::Iterator i(settings); !i.IsAtEnd(); i.Advance()) {
    changes.push_back(ValueStoreChange(i.key(), NULL, i.value().DeepCopy()));
  }

  if (changes.empty())
    return syncer::SyncError();

  syncer::SyncError error = sync_processor_->SendChanges(changes);
  if (error.IsSet())
    StopSyncing();

  return error;
}

syncer::SyncError SyncableSettingsStorage::OverwriteLocalSettingsWithSync(
    const base::DictionaryValue& sync_state,
    const base::DictionaryValue& settings) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  // Treat this as a list of changes to sync and use ProcessSyncChanges.
  // This gives notifications etc for free.
  scoped_ptr<base::DictionaryValue> new_sync_state(sync_state.DeepCopy());

  SettingSyncDataList changes;
  for (base::DictionaryValue::Iterator it(settings);
       !it.IsAtEnd(); it.Advance()) {
    scoped_ptr<base::Value> sync_value;
    if (new_sync_state->RemoveWithoutPathExpansion(it.key(), &sync_value)) {
      if (sync_value->Equals(&it.value())) {
        // Sync and local values are the same, no changes to send.
      } else {
        // Sync value is different, update local setting with new value.
        changes.push_back(
            SettingSyncData(
                syncer::SyncChange::ACTION_UPDATE,
                extension_id_,
                it.key(),
                sync_value.Pass()));
      }
    } else {
      // Not synced, delete local setting.
      changes.push_back(
          SettingSyncData(
              syncer::SyncChange::ACTION_DELETE,
              extension_id_,
              it.key(),
              scoped_ptr<base::Value>(new base::DictionaryValue())));
    }
  }

  // Add all new settings to local settings.
  while (!new_sync_state->empty()) {
    base::DictionaryValue::Iterator first_entry(*new_sync_state);
    std::string key = first_entry.key();
    scoped_ptr<base::Value> value;
    CHECK(new_sync_state->RemoveWithoutPathExpansion(key, &value));
    changes.push_back(
        SettingSyncData(
            syncer::SyncChange::ACTION_ADD,
            extension_id_,
            key,
            value.Pass()));
  }

  if (changes.empty())
    return syncer::SyncError();

  return ProcessSyncChanges(changes);
}

void SyncableSettingsStorage::StopSyncing() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  sync_processor_.reset();
}

syncer::SyncError SyncableSettingsStorage::ProcessSyncChanges(
    const SettingSyncDataList& sync_changes) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(!sync_changes.empty()) << "No sync changes for " << extension_id_;

  if (!sync_processor_.get()) {
    return syncer::SyncError(
        FROM_HERE,
        syncer::SyncError::DATATYPE_ERROR,
        std::string("Sync is inactive for ") + extension_id_,
        syncer::UNSPECIFIED);
  }

  std::vector<syncer::SyncError> errors;
  ValueStoreChangeList changes;

  for (SettingSyncDataList::const_iterator it = sync_changes.begin();
      it != sync_changes.end(); ++it) {
    DCHECK_EQ(extension_id_, it->extension_id());

    const std::string& key = it->key();
    const base::Value& value = it->value();

    scoped_ptr<base::Value> current_value;
    {
      ReadResult maybe_settings = Get(it->key());
      if (maybe_settings->HasError()) {
        errors.push_back(syncer::SyncError(
            FROM_HERE,
            syncer::SyncError::DATATYPE_ERROR,
            base::StringPrintf("Error getting current sync state for %s/%s: %s",
                extension_id_.c_str(), key.c_str(),
                maybe_settings->error().message.c_str()),
            sync_processor_->type()));
        continue;
      }
      maybe_settings->settings().RemoveWithoutPathExpansion(key,
                                                            &current_value);
    }

    syncer::SyncError error;

    switch (it->change_type()) {
      case syncer::SyncChange::ACTION_ADD:
        if (!current_value.get()) {
          error = OnSyncAdd(key, value.DeepCopy(), &changes);
        } else {
          // Already a value; hopefully a local change has beaten sync in a
          // race and it's not a bug, so pretend it's an update.
          LOG(WARNING) << "Got add from sync for existing setting " <<
              extension_id_ << "/" << key;
          error = OnSyncUpdate(
              key, current_value.release(), value.DeepCopy(), &changes);
        }
        break;

      case syncer::SyncChange::ACTION_UPDATE:
        if (current_value.get()) {
          error = OnSyncUpdate(
              key, current_value.release(), value.DeepCopy(), &changes);
        } else {
          // Similarly, pretend it's an add.
          LOG(WARNING) << "Got update from sync for nonexistent setting" <<
              extension_id_ << "/" << key;
          error = OnSyncAdd(key, value.DeepCopy(), &changes);
        }
        break;

      case syncer::SyncChange::ACTION_DELETE:
        if (current_value.get()) {
          error = OnSyncDelete(key, current_value.release(), &changes);
        } else {
          // Similarly, ignore it.
          LOG(WARNING) << "Got delete from sync for nonexistent setting " <<
              extension_id_ << "/" << key;
        }
        break;

      default:
        NOTREACHED();
    }

    if (error.IsSet()) {
      errors.push_back(error);
    }
  }

  sync_processor_->NotifyChanges(changes);

  observers_->Notify(
      &SettingsObserver::OnSettingsChanged,
      extension_id_,
      settings_namespace::SYNC,
      ValueStoreChange::ToJson(changes));

  // TODO(kalman): Something sensible with multiple errors.
  return errors.empty() ? syncer::SyncError() : errors[0];
}

syncer::SyncError SyncableSettingsStorage::OnSyncAdd(
    const std::string& key,
    base::Value* new_value,
    ValueStoreChangeList* changes) {
  DCHECK(new_value);
  WriteResult result = delegate_->Set(IGNORE_QUOTA, key, *new_value);
  if (result->HasError()) {
    return syncer::SyncError(
        FROM_HERE,
        syncer::SyncError::DATATYPE_ERROR,
        base::StringPrintf("Error pushing sync add to local settings: %s",
            result->error().message.c_str()),
        sync_processor_->type());
  }
  changes->push_back(ValueStoreChange(key, NULL, new_value));
  return syncer::SyncError();
}

syncer::SyncError SyncableSettingsStorage::OnSyncUpdate(
    const std::string& key,
    base::Value* old_value,
    base::Value* new_value,
    ValueStoreChangeList* changes) {
  DCHECK(old_value);
  DCHECK(new_value);
  WriteResult result = delegate_->Set(IGNORE_QUOTA, key, *new_value);
  if (result->HasError()) {
    return syncer::SyncError(
        FROM_HERE,
        syncer::SyncError::DATATYPE_ERROR,
        base::StringPrintf("Error pushing sync update to local settings: %s",
            result->error().message.c_str()),
        sync_processor_->type());
  }
  changes->push_back(ValueStoreChange(key, old_value, new_value));
  return syncer::SyncError();
}

syncer::SyncError SyncableSettingsStorage::OnSyncDelete(
    const std::string& key,
    base::Value* old_value,
    ValueStoreChangeList* changes) {
  DCHECK(old_value);
  WriteResult result = delegate_->Remove(key);
  if (result->HasError()) {
    return syncer::SyncError(
        FROM_HERE,
        syncer::SyncError::DATATYPE_ERROR,
        base::StringPrintf("Error pushing sync remove to local settings: %s",
            result->error().message.c_str()),
        sync_processor_->type());
  }
  changes->push_back(ValueStoreChange(key, old_value, NULL));
  return syncer::SyncError();
}

}  // namespace extensions
