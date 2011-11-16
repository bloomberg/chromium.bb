// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/syncable_settings_storage.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/settings/settings_sync_util.h"
#include "chrome/browser/sync/api/sync_data.h"
#include "chrome/browser/sync/protocol/extension_setting_specifics.pb.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

using content::BrowserThread;

SyncableSettingsStorage::SyncableSettingsStorage(
    const scoped_refptr<ObserverListThreadSafe<SettingsObserver> >&
        observers,
    const std::string& extension_id,
    SettingsStorage* delegate)
    : observers_(observers),
      extension_id_(extension_id),
      delegate_(delegate),
      sync_type_(syncable::UNSPECIFIED),
      sync_processor_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

SyncableSettingsStorage::~SyncableSettingsStorage() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

SettingsStorage::ReadResult SyncableSettingsStorage::Get(
    const std::string& key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return delegate_->Get(key);
}

SettingsStorage::ReadResult SyncableSettingsStorage::Get(
    const std::vector<std::string>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return delegate_->Get(keys);
}

SettingsStorage::ReadResult SyncableSettingsStorage::Get() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return delegate_->Get();
}

SettingsStorage::WriteResult SyncableSettingsStorage::Set(
    const std::string& key, const Value& value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  WriteResult result = delegate_->Set(key, value);
  if (result.HasError()) {
    return result;
  }
  if (sync_processor_ && !result.changes().empty()) {
    SendChangesToSync(result.changes());
  }
  return result;
}

SettingsStorage::WriteResult SyncableSettingsStorage::Set(
    const DictionaryValue& values) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  WriteResult result = delegate_->Set(values);
  if (result.HasError()) {
    return result;
  }
  if (sync_processor_ && !result.changes().empty()) {
    SendChangesToSync(result.changes());
  }
  return result;
}

SettingsStorage::WriteResult SyncableSettingsStorage::Remove(
    const std::string& key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  WriteResult result = delegate_->Remove(key);
  if (result.HasError()) {
    return result;
  }
  if (sync_processor_ && !result.changes().empty()) {
    SendChangesToSync(result.changes());
  }
  return result;
}

SettingsStorage::WriteResult SyncableSettingsStorage::Remove(
    const std::vector<std::string>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  WriteResult result = delegate_->Remove(keys);
  if (result.HasError()) {
    return result;
  }
  if (sync_processor_ && !result.changes().empty()) {
    SendChangesToSync(result.changes());
  }
  return result;
}

SettingsStorage::WriteResult
SyncableSettingsStorage::Clear() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  WriteResult result = delegate_->Clear();
  if (result.HasError()) {
    return result;
  }
  if (sync_processor_ && !result.changes().empty()) {
    SendChangesToSync(result.changes());
  }
  return result;
}

// Sync-related methods.

SyncError SyncableSettingsStorage::StartSyncing(
    syncable::ModelType type,
    const DictionaryValue& sync_state,
    SyncChangeProcessor* sync_processor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(type == syncable::EXTENSION_SETTINGS ||
         type == syncable::APP_SETTINGS);
  DCHECK_EQ(sync_type_, syncable::UNSPECIFIED);
  DCHECK(!sync_processor_);
  DCHECK(synced_keys_.empty());

  sync_type_ = type;
  sync_processor_ = sync_processor;

  ReadResult maybe_settings = delegate_->Get();
  if (maybe_settings.HasError()) {
    return SyncError(
        FROM_HERE,
        std::string("Failed to get settings: ") + maybe_settings.error(),
        type);
  }

  if (sync_state.empty()) {
    return SendLocalSettingsToSync(maybe_settings.settings());
  }
  return OverwriteLocalSettingsWithSync(sync_state, maybe_settings.settings());
}

SyncError SyncableSettingsStorage::SendLocalSettingsToSync(
    const DictionaryValue& settings) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(sync_processor_);

  SyncChangeList changes;
  for (DictionaryValue::key_iterator it = settings.begin_keys();
      it != settings.end_keys(); ++it) {
    Value* value;
    settings.GetWithoutPathExpansion(*it, &value);
    changes.push_back(
        settings_sync_util::CreateAdd(extension_id_, *it, *value));
  }

  if (changes.empty()) {
    return SyncError();
  }

  SyncError error = sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
  if (error.IsSet()) {
    StopSyncing();
    return error;
  }

  for (DictionaryValue::key_iterator it = settings.begin_keys();
      it != settings.end_keys(); ++it) {
    synced_keys_.insert(*it);
  }
  return SyncError();
}

SyncError SyncableSettingsStorage::OverwriteLocalSettingsWithSync(
    const DictionaryValue& sync_state, const DictionaryValue& settings) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // Treat this as a list of changes to sync and use ProcessSyncChanges.
  // This gives notifications etc for free.
  scoped_ptr<DictionaryValue> new_sync_state(sync_state.DeepCopy());

  SettingSyncDataList changes;
  for (DictionaryValue::key_iterator it = settings.begin_keys();
      it != settings.end_keys(); ++it) {
    Value* orphaned_sync_value = NULL;
    if (new_sync_state->RemoveWithoutPathExpansion(*it, &orphaned_sync_value)) {
      scoped_ptr<Value> sync_value(orphaned_sync_value);
      Value* local_value = NULL;
      settings.GetWithoutPathExpansion(*it, &local_value);
      if (sync_value->Equals(local_value)) {
        // Sync and local values are the same, no changes to send.
        synced_keys_.insert(*it);
      } else {
        // Sync value is different, update local setting with new value.
        changes.push_back(
            SettingSyncData(
                SyncChange::ACTION_UPDATE,
                extension_id_,
                *it,
                sync_value.release()));
      }
    } else {
      // Not synced, delete local setting.
      changes.push_back(
          SettingSyncData(
              SyncChange::ACTION_DELETE,
              extension_id_,
              *it,
              new DictionaryValue()));
    }
  }

  // Add all new settings to local settings.
  while (!new_sync_state->empty()) {
    std::string key = *new_sync_state->begin_keys();
    Value* value = NULL;
    CHECK(new_sync_state->RemoveWithoutPathExpansion(key, &value));
    changes.push_back(
        SettingSyncData(
            SyncChange::ACTION_ADD, extension_id_, key, value));
  }

  if (changes.empty()) {
    return SyncError();
  }
  return ProcessSyncChanges(changes);
}

void SyncableSettingsStorage::StopSyncing() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Stop syncing is allowed to be called multiple times without StartSyncing,
  // so don't DCHECK that these values aren't already disabled.
  sync_type_ = syncable::UNSPECIFIED;
  sync_processor_ = NULL;
  synced_keys_.clear();
}

SyncError SyncableSettingsStorage::ProcessSyncChanges(
    const SettingSyncDataList& sync_changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!sync_changes.empty()) << "No sync changes for " << extension_id_;

  if (!sync_processor_) {
    return SyncError(
        FROM_HERE,
        std::string("Sync is inactive for ") + extension_id_,
        sync_type_);
  }

  std::vector<SyncError> errors;
  SettingChangeList changes;

  for (SettingSyncDataList::const_iterator it = sync_changes.begin();
      it != sync_changes.end(); ++it) {
    DCHECK_EQ(extension_id_, it->extension_id());

    const std::string& key = it->key();
    const Value& value = it->value();

    scoped_ptr<Value> current_value;
    {
      ReadResult maybe_settings = Get(it->key());
      if (maybe_settings.HasError()) {
        errors.push_back(SyncError(
            FROM_HERE,
            std::string("Error getting current sync state for ") +
                extension_id_ + "/" + key + ": " + maybe_settings.error(),
            sync_type_));
        continue;
      }
      Value* value = NULL;
      if (maybe_settings.settings().GetWithoutPathExpansion(key, &value)) {
        current_value.reset(value->DeepCopy());
      }
    }

    SyncError error;

    switch (it->change_type()) {
      case SyncChange::ACTION_ADD:
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

      case SyncChange::ACTION_UPDATE:
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

      case SyncChange::ACTION_DELETE:
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

  observers_->Notify(
      &SettingsObserver::OnSettingsChanged,
      extension_id_,
      SettingChange::GetEventJson(changes));

  // TODO(kalman): Something sensible with multiple errors.
  return errors.empty() ? SyncError() : errors[0];
}

void SyncableSettingsStorage::SendChangesToSync(
    const SettingChangeList& changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(sync_processor_);
  DCHECK(!changes.empty());

  SyncChangeList sync_changes;
  std::set<std::string> added_keys;
  std::set<std::string> deleted_keys;

  for (SettingChangeList::const_iterator it = changes.begin();
      it != changes.end(); ++it) {
    const std::string& key = it->key();
    const Value* value = it->new_value();
    if (value) {
      if (synced_keys_.count(key)) {
        // New value, key is synced; send ACTION_UPDATE.
        sync_changes.push_back(
            settings_sync_util::CreateUpdate(
                extension_id_, key, *value));
      } else {
        // New value, key is not synced; send ACTION_ADD.
        sync_changes.push_back(
            settings_sync_util::CreateAdd(
                extension_id_, key, *value));
        added_keys.insert(key);
      }
    } else {
      if (synced_keys_.count(key)) {
        // Clearing value, key is synced; send ACTION_DELETE.
        sync_changes.push_back(
            settings_sync_util::CreateDelete(extension_id_, key));
        deleted_keys.insert(key);
      } else {
        LOG(WARNING) << "Deleted " << key << " but not in synced_keys_";
      }
    }
  }

  if (sync_changes.empty()) {
    return;
  }

  SyncError error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, sync_changes);
  if (error.IsSet()) {
    StopSyncing();
    return;
  }

  synced_keys_.insert(added_keys.begin(), added_keys.end());
  for (std::set<std::string>::iterator it = deleted_keys.begin();
      it != deleted_keys.end(); ++it) {
    synced_keys_.erase(*it);
  }
}

SyncError SyncableSettingsStorage::OnSyncAdd(
    const std::string& key,
    Value* new_value,
    SettingChangeList* changes) {
  DCHECK(new_value);
  synced_keys_.insert(key);
  WriteResult result = delegate_->Set(key, *new_value);
  if (result.HasError()) {
    return SyncError(
        FROM_HERE,
        std::string("Error pushing sync add to local settings: ") +
            result.error(),
        sync_type_);
  }
  changes->push_back(SettingChange(key, NULL, new_value));
  return SyncError();
}

SyncError SyncableSettingsStorage::OnSyncUpdate(
    const std::string& key,
    Value* old_value,
    Value* new_value,
    SettingChangeList* changes) {
  DCHECK(old_value);
  DCHECK(new_value);
  WriteResult result = delegate_->Set(key, *new_value);
  if (result.HasError()) {
    return SyncError(
        FROM_HERE,
        std::string("Error pushing sync update to local settings: ") +
            result.error(),
        sync_type_);
  }
  changes->push_back(SettingChange(key, old_value, new_value));
  return SyncError();
}

SyncError SyncableSettingsStorage::OnSyncDelete(
    const std::string& key,
    Value* old_value,
    SettingChangeList* changes) {
  DCHECK(old_value);
  synced_keys_.erase(key);
  WriteResult result = delegate_->Remove(key);
  if (result.HasError()) {
    return SyncError(
        FROM_HERE,
        std::string("Error pushing sync remove to local settings: ") +
            result.error(),
        sync_type_);
  }
  changes->push_back(SettingChange(key, old_value, NULL));
  return SyncError();
}

}  // namespace extensions
