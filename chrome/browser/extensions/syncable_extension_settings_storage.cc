// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/syncable_extension_settings_storage.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_settings_sync_util.h"
#include "chrome/browser/sync/api/sync_data.h"
#include "chrome/browser/sync/protocol/extension_setting_specifics.pb.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

SyncableExtensionSettingsStorage::SyncableExtensionSettingsStorage(
    const scoped_refptr<ObserverListThreadSafe<ExtensionSettingsObserver> >&
        observers,
    const std::string& extension_id,
    ExtensionSettingsStorage* delegate)
    : observers_(observers),
      extension_id_(extension_id),
      delegate_(delegate),
      sync_type_(syncable::UNSPECIFIED),
      sync_processor_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

SyncableExtensionSettingsStorage::~SyncableExtensionSettingsStorage() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

ExtensionSettingsStorage::ReadResult SyncableExtensionSettingsStorage::Get(
    const std::string& key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return delegate_->Get(key);
}

ExtensionSettingsStorage::ReadResult SyncableExtensionSettingsStorage::Get(
    const std::vector<std::string>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return delegate_->Get(keys);
}

ExtensionSettingsStorage::ReadResult SyncableExtensionSettingsStorage::Get() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return delegate_->Get();
}

ExtensionSettingsStorage::WriteResult SyncableExtensionSettingsStorage::Set(
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

ExtensionSettingsStorage::WriteResult SyncableExtensionSettingsStorage::Set(
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

ExtensionSettingsStorage::WriteResult SyncableExtensionSettingsStorage::Remove(
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

ExtensionSettingsStorage::WriteResult SyncableExtensionSettingsStorage::Remove(
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

ExtensionSettingsStorage::WriteResult
SyncableExtensionSettingsStorage::Clear() {
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

SyncError SyncableExtensionSettingsStorage::StartSyncing(
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

SyncError SyncableExtensionSettingsStorage::SendLocalSettingsToSync(
    const DictionaryValue& settings) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  SyncChangeList changes;
  for (DictionaryValue::key_iterator it = settings.begin_keys();
      it != settings.end_keys(); ++it) {
    Value* value;
    settings.GetWithoutPathExpansion(*it, &value);
    changes.push_back(
        extension_settings_sync_util::CreateAdd(extension_id_, *it, *value));
  }

  if (changes.empty()) {
    return SyncError();
  }

  SyncError error = sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
  if (error.IsSet()) {
    return error;
  }

  for (DictionaryValue::key_iterator it = settings.begin_keys();
      it != settings.end_keys(); ++it) {
    synced_keys_.insert(*it);
  }
  return SyncError();
}

SyncError SyncableExtensionSettingsStorage::OverwriteLocalSettingsWithSync(
    const DictionaryValue& sync_state, const DictionaryValue& settings) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // Treat this as a list of changes to sync and use ProcessSyncChanges.
  // This gives notifications etc for free.
  scoped_ptr<DictionaryValue> new_sync_state(sync_state.DeepCopy());

  ExtensionSettingSyncDataList changes;
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
            ExtensionSettingSyncData(
                SyncChange::ACTION_UPDATE,
                extension_id_,
                *it,
                sync_value.release()));
      }
    } else {
      // Not synced, delete local setting.
      changes.push_back(
          ExtensionSettingSyncData(
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
        ExtensionSettingSyncData(
            SyncChange::ACTION_ADD, extension_id_, key, value));
  }

  if (changes.empty()) {
    return SyncError();
  }

  std::vector<SyncError> sync_errors(ProcessSyncChanges(changes));
  if (sync_errors.empty()) {
    return SyncError();
  }
  // TODO(kalman): something sensible with multiple errors.
  return sync_errors[0];
}

void SyncableExtensionSettingsStorage::StopSyncing() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(sync_type_ == syncable::EXTENSION_SETTINGS ||
         sync_type_ == syncable::APP_SETTINGS);
  DCHECK(sync_processor_);

  sync_type_ = syncable::UNSPECIFIED;
  sync_processor_ = NULL;
  synced_keys_.clear();
}

std::vector<SyncError> SyncableExtensionSettingsStorage::ProcessSyncChanges(
    const ExtensionSettingSyncDataList& sync_changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(sync_processor_);
  DCHECK(!sync_changes.empty()) << "No sync changes for " << extension_id_;

  std::vector<SyncError> errors;
  ExtensionSettingChangeList changes;

  for (ExtensionSettingSyncDataList::const_iterator it = sync_changes.begin();
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
      &ExtensionSettingsObserver::OnSettingsChanged,
      extension_id_,
      ExtensionSettingChange::GetEventJson(changes));

  return errors;
}

void SyncableExtensionSettingsStorage::SendChangesToSync(
    const ExtensionSettingChangeList& changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(sync_processor_);
  DCHECK(!changes.empty());

  SyncChangeList sync_changes;
  std::set<std::string> added_keys;
  std::set<std::string> deleted_keys;

  for (ExtensionSettingChangeList::const_iterator it = changes.begin();
      it != changes.end(); ++it) {
    const std::string& key = it->key();
    const Value* value = it->new_value();
    if (value) {
      if (synced_keys_.count(key)) {
        // New value, key is synced; send ACTION_UPDATE.
        sync_changes.push_back(
            extension_settings_sync_util::CreateUpdate(
                extension_id_, key, *value));
      } else {
        // New value, key is not synced; send ACTION_ADD.
        sync_changes.push_back(
            extension_settings_sync_util::CreateAdd(
                extension_id_, key, *value));
        added_keys.insert(key);
      }
    } else {
      if (synced_keys_.count(key)) {
        // Clearing value, key is synced; send ACTION_DELETE.
        sync_changes.push_back(
            extension_settings_sync_util::CreateDelete(extension_id_, key));
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
    LOG(WARNING) << "Failed to send changes to sync: " << error.message();
    return;
  }

  synced_keys_.insert(added_keys.begin(), added_keys.end());
  for (std::set<std::string>::iterator it = deleted_keys.begin();
      it != deleted_keys.end(); ++it) {
    synced_keys_.erase(*it);
  }
}

SyncError SyncableExtensionSettingsStorage::OnSyncAdd(
    const std::string& key,
    Value* new_value,
    ExtensionSettingChangeList* changes) {
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
  changes->push_back(ExtensionSettingChange(key, NULL, new_value));
  return SyncError();
}

SyncError SyncableExtensionSettingsStorage::OnSyncUpdate(
    const std::string& key,
    Value* old_value,
    Value* new_value,
    ExtensionSettingChangeList* changes) {
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
  changes->push_back(ExtensionSettingChange(key, old_value, new_value));
  return SyncError();
}

SyncError SyncableExtensionSettingsStorage::OnSyncDelete(
    const std::string& key,
    Value* old_value,
    ExtensionSettingChangeList* changes) {
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
  changes->push_back(ExtensionSettingChange(key, old_value, NULL));
  return SyncError();
}
