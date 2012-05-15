// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/syncable_settings_storage.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/settings/settings_namespace.h"
#include "chrome/browser/extensions/settings/settings_sync_processor.h"
#include "chrome/browser/extensions/settings/settings_sync_util.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_data.h"
#include "sync/protocol/extension_setting_specifics.pb.h"

namespace extensions {

using content::BrowserThread;

SyncableSettingsStorage::SyncableSettingsStorage(
    const scoped_refptr<ObserverListThreadSafe<SettingsObserver> >&
        observers,
    const std::string& extension_id,
    SettingsStorage* delegate)
    : observers_(observers),
      extension_id_(extension_id),
      delegate_(delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

SyncableSettingsStorage::~SyncableSettingsStorage() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

size_t SyncableSettingsStorage::GetBytesInUse(const std::string& key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return delegate_->GetBytesInUse(key);
}

size_t SyncableSettingsStorage::GetBytesInUse(
    const std::vector<std::string>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return delegate_->GetBytesInUse(keys);
}

size_t SyncableSettingsStorage::GetBytesInUse() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return delegate_->GetBytesInUse();
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
    WriteOptions options, const std::string& key, const Value& value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  WriteResult result = delegate_->Set(options, key, value);
  if (result.HasError()) {
    return result;
  }
  SyncResultIfEnabled(result);
  return result;
}

SettingsStorage::WriteResult SyncableSettingsStorage::Set(
    WriteOptions options, const DictionaryValue& values) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  WriteResult result = delegate_->Set(options, values);
  if (result.HasError()) {
    return result;
  }
  SyncResultIfEnabled(result);
  return result;
}

SettingsStorage::WriteResult SyncableSettingsStorage::Remove(
    const std::string& key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  WriteResult result = delegate_->Remove(key);
  if (result.HasError()) {
    return result;
  }
  SyncResultIfEnabled(result);
  return result;
}

SettingsStorage::WriteResult SyncableSettingsStorage::Remove(
    const std::vector<std::string>& keys) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  WriteResult result = delegate_->Remove(keys);
  if (result.HasError()) {
    return result;
  }
  SyncResultIfEnabled(result);
  return result;
}

SettingsStorage::WriteResult SyncableSettingsStorage::Clear() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  WriteResult result = delegate_->Clear();
  if (result.HasError()) {
    return result;
  }
  SyncResultIfEnabled(result);
  return result;
}

void SyncableSettingsStorage::SyncResultIfEnabled(
    SettingsStorage::WriteResult result) {
  if (sync_processor_.get() && !result.changes().empty()) {
    SyncError error = sync_processor_->SendChanges(result.changes());
    if (error.IsSet())
      StopSyncing();
  }
}

// Sync-related methods.

SyncError SyncableSettingsStorage::StartSyncing(
    const DictionaryValue& sync_state,
    scoped_ptr<SettingsSyncProcessor> sync_processor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!sync_processor_.get());

  sync_processor_ = sync_processor.Pass();
  sync_processor_->Init(sync_state);

  ReadResult maybe_settings = delegate_->Get();
  if (maybe_settings.HasError()) {
    return SyncError(
        FROM_HERE,
        std::string("Failed to get settings: ") + maybe_settings.error(),
        sync_processor_->type());
  }

  const DictionaryValue& settings = maybe_settings.settings();
  if (sync_state.empty())
    return SendLocalSettingsToSync(settings);
  else
    return OverwriteLocalSettingsWithSync(sync_state, settings);
}

SyncError SyncableSettingsStorage::SendLocalSettingsToSync(
    const DictionaryValue& settings) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  SettingChangeList changes;
  for (DictionaryValue::Iterator i(settings); i.HasNext(); i.Advance()) {
    changes.push_back(SettingChange(i.key(), NULL, i.value().DeepCopy()));
  }

  if (changes.empty())
    return SyncError();

  SyncError error = sync_processor_->SendChanges(changes);
  if (error.IsSet())
    StopSyncing();

  return error;
}

SyncError SyncableSettingsStorage::OverwriteLocalSettingsWithSync(
    const DictionaryValue& sync_state, const DictionaryValue& settings) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // Treat this as a list of changes to sync and use ProcessSyncChanges.
  // This gives notifications etc for free.
  scoped_ptr<DictionaryValue> new_sync_state(sync_state.DeepCopy());

  SettingSyncDataList changes;
  for (DictionaryValue::Iterator it(settings); it.HasNext(); it.Advance()) {
    Value* orphaned_sync_value = NULL;
    if (new_sync_state->RemoveWithoutPathExpansion(
          it.key(), &orphaned_sync_value)) {
      scoped_ptr<Value> sync_value(orphaned_sync_value);
      if (sync_value->Equals(&it.value())) {
        // Sync and local values are the same, no changes to send.
      } else {
        // Sync value is different, update local setting with new value.
        changes.push_back(
            SettingSyncData(
                SyncChange::ACTION_UPDATE,
                extension_id_,
                it.key(),
                sync_value.Pass()));
      }
    } else {
      // Not synced, delete local setting.
      changes.push_back(
          SettingSyncData(
              SyncChange::ACTION_DELETE,
              extension_id_,
              it.key(),
              scoped_ptr<Value>(new DictionaryValue())));
    }
  }

  // Add all new settings to local settings.
  while (!new_sync_state->empty()) {
    std::string key = *new_sync_state->begin_keys();
    Value* value = NULL;
    CHECK(new_sync_state->RemoveWithoutPathExpansion(key, &value));
    changes.push_back(
        SettingSyncData(
            SyncChange::ACTION_ADD,
            extension_id_,
            key,
            scoped_ptr<Value>(value)));
  }

  if (changes.empty())
    return SyncError();

  return ProcessSyncChanges(changes);
}

void SyncableSettingsStorage::StopSyncing() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  sync_processor_.reset();
}

SyncError SyncableSettingsStorage::ProcessSyncChanges(
    const SettingSyncDataList& sync_changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!sync_changes.empty()) << "No sync changes for " << extension_id_;

  if (!sync_processor_.get()) {
    return SyncError(
        FROM_HERE,
        std::string("Sync is inactive for ") + extension_id_,
        syncable::UNSPECIFIED);
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
            sync_processor_->type()));
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

  sync_processor_->NotifyChanges(changes);

  observers_->Notify(
      &SettingsObserver::OnSettingsChanged,
      extension_id_,
      settings_namespace::SYNC,
      SettingChange::GetEventJson(changes));

  // TODO(kalman): Something sensible with multiple errors.
  return errors.empty() ? SyncError() : errors[0];
}

SyncError SyncableSettingsStorage::OnSyncAdd(
    const std::string& key,
    Value* new_value,
    SettingChangeList* changes) {
  DCHECK(new_value);
  WriteResult result = delegate_->Set(IGNORE_QUOTA, key, *new_value);
  if (result.HasError()) {
    return SyncError(
        FROM_HERE,
        std::string("Error pushing sync add to local settings: ") +
            result.error(),
        sync_processor_->type());
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
  WriteResult result = delegate_->Set(IGNORE_QUOTA, key, *new_value);
  if (result.HasError()) {
    return SyncError(
        FROM_HERE,
        std::string("Error pushing sync update to local settings: ") +
            result.error(),
        sync_processor_->type());
  }
  changes->push_back(SettingChange(key, old_value, new_value));
  return SyncError();
}

SyncError SyncableSettingsStorage::OnSyncDelete(
    const std::string& key,
    Value* old_value,
    SettingChangeList* changes) {
  DCHECK(old_value);
  WriteResult result = delegate_->Remove(key);
  if (result.HasError()) {
    return SyncError(
        FROM_HERE,
        std::string("Error pushing sync remove to local settings: ") +
            result.error(),
        sync_processor_->type());
  }
  changes->push_back(SettingChange(key, old_value, NULL));
  return SyncError();
}

}  // namespace extensions
