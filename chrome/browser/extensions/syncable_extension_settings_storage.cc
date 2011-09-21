// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/syncable_extension_settings_storage.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_settings_sync_util.h"
#include "chrome/browser/sync/api/sync_data.h"
#include "chrome/browser/sync/protocol/extension_setting_specifics.pb.h"
#include "content/browser/browser_thread.h"

namespace {

// Inserts all the keys from a dictionary into a set of strings.
void InsertAll(const DictionaryValue& src, std::set<std::string>* dst) {
  for (DictionaryValue::key_iterator it = src.begin_keys();
      it != src.end_keys(); ++it) {
    dst->insert(*it);
  }
}

}  // namespace

SyncableExtensionSettingsStorage::SyncableExtensionSettingsStorage(
    std::string extension_id, ExtensionSettingsStorage* delegate)
    : extension_id_(extension_id), delegate_(delegate), sync_processor_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

SyncableExtensionSettingsStorage::~SyncableExtensionSettingsStorage() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

ExtensionSettingsStorage::Result SyncableExtensionSettingsStorage::Get(
    const std::string& key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return delegate_->Get(key);
}

ExtensionSettingsStorage::Result SyncableExtensionSettingsStorage::Get(
    const std::vector<std::string>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return delegate_->Get(keys);
}

ExtensionSettingsStorage::Result SyncableExtensionSettingsStorage::Get() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return delegate_->Get();
}

ExtensionSettingsStorage::Result SyncableExtensionSettingsStorage::Set(
    const std::string& key, const Value& value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Result result = delegate_->Set(key, value);
  if (result.HasError()) {
    return result;
  }
  if (sync_processor_) {
    SendAddsOrUpdatesToSync(*result.GetSettings());
  }
  return result;
}

ExtensionSettingsStorage::Result SyncableExtensionSettingsStorage::Set(
    const DictionaryValue& values) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Result result = delegate_->Set(values);
  if (result.HasError()) {
    return result;
  }
  if (sync_processor_) {
    SendAddsOrUpdatesToSync(*result.GetSettings());
  }
  return result;
}

ExtensionSettingsStorage::Result SyncableExtensionSettingsStorage::Remove(
    const std::string& key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Result result = delegate_->Remove(key);
  if (result.HasError()) {
    return result;
  }
  if (sync_processor_) {
    std::vector<std::string> keys;
    keys.push_back(key);
    SendDeletesToSync(keys);
  }
  return result;
}

ExtensionSettingsStorage::Result SyncableExtensionSettingsStorage::Remove(
    const std::vector<std::string>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Result result = delegate_->Remove(keys);
  if (result.HasError()) {
    return result;
  }
  if (sync_processor_) {
    SendDeletesToSync(keys);
  }
  return result;
}

ExtensionSettingsStorage::Result SyncableExtensionSettingsStorage::Clear() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Result result = delegate_->Clear();
  if (result.HasError()) {
    return result;
  }
  if (sync_processor_) {
    SendDeletesToSync(
        std::vector<std::string>(synced_keys_.begin(), synced_keys_.end()));
  }
  return result;
}

// Sync-related methods.

SyncError SyncableExtensionSettingsStorage::StartSyncing(
    const DictionaryValue& sync_state, SyncChangeProcessor* sync_processor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!sync_processor_);
  DCHECK(synced_keys_.empty());
  sync_processor_ = sync_processor;
  InsertAll(sync_state, &synced_keys_);

  Result maybe_settings = delegate_->Get();
  if (maybe_settings.HasError()) {
    return SyncError(
        FROM_HERE,
        std::string("Failed to get settings: ") + maybe_settings.GetError(),
        syncable::EXTENSION_SETTINGS);
  }

  DictionaryValue* settings = maybe_settings.GetSettings();
  if (sync_state.empty()) {
    return SendLocalSettingsToSync(*settings);
  }
  return OverwriteLocalSettingsWithSync(sync_state, *settings);
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

  InsertAll(settings, &synced_keys_);
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
    Value* sync_value = NULL;
    if (new_sync_state->RemoveWithoutPathExpansion(*it, &sync_value)) {
      Value* local_value = NULL;
      settings.GetWithoutPathExpansion(*it, &local_value);
      if (!local_value->Equals(sync_value)) {
        // Sync value is different, update local setting with new value.
        changes.push_back(
            ExtensionSettingSyncData(
                SyncChange::ACTION_UPDATE, extension_id_, *it, sync_value));
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
    Value* value;
    new_sync_state->RemoveWithoutPathExpansion(key, &value);
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
  DCHECK(sync_processor_);
  sync_processor_ = NULL;
  synced_keys_.clear();
}

std::vector<SyncError> SyncableExtensionSettingsStorage::ProcessSyncChanges(
    const ExtensionSettingSyncDataList& sync_changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(sync_processor_);
  DCHECK(!sync_changes.empty()) << "No sync changes for " << extension_id_;

  std::vector<SyncError> errors;

  for (ExtensionSettingSyncDataList::const_iterator it = sync_changes.begin();
      it != sync_changes.end(); ++it) {
    DCHECK_EQ(extension_id_, it->extension_id());
    switch (it->change_type()) {
      case SyncChange::ACTION_ADD:
      case SyncChange::ACTION_UPDATE: {
        synced_keys_.insert(it->key());
        Result result = delegate_->Set(it->key(), it->value());
        if (result.HasError()) {
          errors.push_back(SyncError(
              FROM_HERE,
              std::string("Error pushing sync change to local settings: ") +
                  result.GetError(),
              syncable::EXTENSION_SETTINGS));
        }
        break;
      }

      case SyncChange::ACTION_DELETE: {
        synced_keys_.erase(it->key());
        Result result = delegate_->Remove(it->key());
        if (result.HasError()) {
          errors.push_back(SyncError(
              FROM_HERE,
              std::string("Error removing sync change from local settings: ") +
                  result.GetError(),
              syncable::EXTENSION_SETTINGS));
        }
        break;
      }

      default:
        NOTREACHED();
    }
  }

  return errors;
}

void SyncableExtensionSettingsStorage::SendAddsOrUpdatesToSync(
    const DictionaryValue& settings) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(sync_processor_);

  SyncChangeList changes;
  for (DictionaryValue::key_iterator it = settings.begin_keys();
      it != settings.end_keys(); ++it) {
    Value* value = NULL;
    settings.GetWithoutPathExpansion(*it, &value);
    DCHECK(value);
    if (synced_keys_.count(*it)) {
      // Key is synced, send ACTION_UPDATE to sync.
      changes.push_back(
          extension_settings_sync_util::CreateUpdate(
              extension_id_, *it, *value));
    } else {
      // Key is not synced, send ACTION_ADD to sync.
      changes.push_back(
          extension_settings_sync_util::CreateAdd(extension_id_, *it, *value));
    }
  }

  if (changes.empty()) {
    return;
  }

  SyncError error = sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
  if (error.IsSet()) {
    LOG(WARNING) << "Failed to send changes to sync: " << error.message();
    return;
  }

  InsertAll(settings, &synced_keys_);
}

void SyncableExtensionSettingsStorage::SendDeletesToSync(
    const std::vector<std::string>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(sync_processor_);

  SyncChangeList changes;
  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    // Only remove from sync if it's actually synced.
    if (synced_keys_.count(*it)) {
      changes.push_back(
          extension_settings_sync_util::CreateDelete(extension_id_, *it));
    }
  }

  if (changes.empty()) {
    return;
  }

  SyncError error = sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
  if (error.IsSet()) {
    LOG(WARNING) << "Failed to send changes to sync: " << error.message();
    return;
  }

  for (std::vector<std::string>::const_iterator it = keys.begin();
      it != keys.end(); ++it) {
    synced_keys_.erase(*it);
  }
}
