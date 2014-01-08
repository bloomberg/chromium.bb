// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_shared_settings_service.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_merge_result.h"
#include "sync/protocol/sync.pb.h"

using base::DictionaryValue;
using base::Value;
using syncer::MANAGED_USER_SHARED_SETTINGS;
using syncer::ModelType;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncChangeProcessor;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using syncer::SyncErrorFactory;
using syncer::SyncMergeResult;

namespace {

const char kAcknowledged[] = "acknowledged";
const char kValue[] = "value";

DictionaryValue* FindOrCreateDictionary(DictionaryValue* parent,
                                        const std::string& key) {
  DictionaryValue* dict = NULL;
  if (!parent->GetDictionaryWithoutPathExpansion(key, &dict)) {
    dict = new DictionaryValue;
    parent->SetWithoutPathExpansion(key, dict);
  }
  return dict;
}

class ScopedManagedUserSharedSettingsUpdate {
 public:
  ScopedManagedUserSharedSettingsUpdate(PrefService* prefs,
                                        const std::string& mu_id)
      : update_(prefs, prefs::kManagedUserSharedSettings), mu_id_(mu_id) {
    DCHECK(!mu_id.empty());

    // A supervised user can only modify their own settings.
    std::string id = prefs->GetString(prefs::kManagedUserId);
    DCHECK(id.empty() || id == mu_id);
  }

  DictionaryValue* Get() {
    return FindOrCreateDictionary(update_.Get(), mu_id_);
  }

 private:
  DictionaryPrefUpdate update_;
  std::string mu_id_;
};

SyncData CreateSyncDataForValue(
    const std::string& mu_id,
    const std::string& key,
    const Value& dict_value) {
  const DictionaryValue* dict = NULL;
  if (!dict_value.GetAsDictionary(&dict))
    return SyncData();

  const Value* value = NULL;
  if (!dict->Get(kValue, &value))
    return SyncData();

  bool acknowledged = false;
  dict->GetBoolean(kAcknowledged, &acknowledged);

  return ManagedUserSharedSettingsService::CreateSyncDataForSetting(
      mu_id, key, *value, acknowledged);
}

}  // namespace


ManagedUserSharedSettingsService::ManagedUserSharedSettingsService(
    PrefService* prefs)
    : prefs_(prefs) {}

ManagedUserSharedSettingsService::~ManagedUserSharedSettingsService() {}

void ManagedUserSharedSettingsService::SetValueInternal(
    const std::string& mu_id,
    const std::string& key,
    const Value& value,
    bool acknowledged) {
  ScopedManagedUserSharedSettingsUpdate update(prefs_, mu_id);
  DictionaryValue* update_dict = update.Get();

  DictionaryValue* dict = NULL;
  bool has_key = update_dict->GetDictionaryWithoutPathExpansion(key, &dict);
  if (!has_key) {
    dict = new DictionaryValue;
    update_dict->SetWithoutPathExpansion(key, dict);
  }
  dict->SetWithoutPathExpansion(kValue, value.DeepCopy());
  dict->SetBooleanWithoutPathExpansion(kAcknowledged, acknowledged);

  if (!sync_processor_)
    return;

  SyncData data = CreateSyncDataForSetting(mu_id, key, value, acknowledged);
  SyncChange::SyncChangeType change_type =
      has_key ? SyncChange::ACTION_UPDATE : SyncChange::ACTION_ADD;
  SyncChangeList changes;
  changes.push_back(SyncChange(FROM_HERE, change_type, data));
  SyncError error = sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
  DCHECK(!error.IsSet()) << error.ToString();
}

const Value* ManagedUserSharedSettingsService::GetValue(
    const std::string& mu_id,
    const std::string& key) {
  const DictionaryValue* data =
      prefs_->GetDictionary(prefs::kManagedUserSharedSettings);
  const DictionaryValue* dict = NULL;
  if (!data->GetDictionaryWithoutPathExpansion(mu_id, &dict))
    return NULL;

  const DictionaryValue* settings = NULL;
  if (!dict->GetDictionaryWithoutPathExpansion(key, &settings))
    return NULL;

  const Value* value = NULL;
  if (!settings->GetWithoutPathExpansion(kValue, &value))
    return NULL;

  return value;
}

void ManagedUserSharedSettingsService::SetValue(
    const std::string& mu_id,
    const std::string& key,
    const Value& value) {
  SetValueInternal(mu_id, key, value, true);
}

scoped_ptr<ManagedUserSharedSettingsService::ChangeCallbackList::Subscription>
ManagedUserSharedSettingsService::Subscribe(
    const ManagedUserSharedSettingsService::ChangeCallback& cb) {
  return callbacks_.Add(cb);
}

// static
void ManagedUserSharedSettingsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kManagedUserSharedSettings,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
SyncData ManagedUserSharedSettingsService::CreateSyncDataForSetting(
    const std::string& mu_id,
    const std::string& key,
    const Value& value,
    bool acknowledged) {
  std::string json_value;
  base::JSONWriter::Write(&value, &json_value);
  ::sync_pb::EntitySpecifics specifics;
  specifics.mutable_managed_user_shared_setting()->set_mu_id(mu_id);
  specifics.mutable_managed_user_shared_setting()->set_key(key);
  specifics.mutable_managed_user_shared_setting()->set_value(json_value);
  specifics.mutable_managed_user_shared_setting()->set_acknowledged(
      acknowledged);
  std::string title = mu_id + ":" + key;
  return SyncData::CreateLocalData(title, title, specifics);
}

void ManagedUserSharedSettingsService::Shutdown() {}

syncer::SyncMergeResult
ManagedUserSharedSettingsService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK_EQ(MANAGED_USER_SHARED_SETTINGS, type);
  sync_processor_ = sync_processor.Pass();
  error_handler_ = error_handler.Pass();

  // We keep a map from MU ID to the set of keys that we have seen in the
  // initial sync data.
  std::map<std::string, std::set<std::string> > seen_keys;

  // Iterate over all initial sync data, and update it locally. This means that
  // the value from the server always wins over a local value.
  for (SyncDataList::const_iterator it = initial_sync_data.begin();
       it != initial_sync_data.end();
       ++it) {
    DCHECK_EQ(MANAGED_USER_SHARED_SETTINGS, it->GetDataType());
    const ::sync_pb::ManagedUserSharedSettingSpecifics&
        managed_user_shared_setting =
            it->GetSpecifics().managed_user_shared_setting();
    scoped_ptr<Value> value(
        base::JSONReader::Read(managed_user_shared_setting.value()));
    const std::string& mu_id = managed_user_shared_setting.mu_id();
    ScopedManagedUserSharedSettingsUpdate update(prefs_, mu_id);
    const std::string& key = managed_user_shared_setting.key();
    DictionaryValue* dict = FindOrCreateDictionary(update.Get(), key);
    dict->SetWithoutPathExpansion(kValue, value.release());

    // Every setting we get from the server should have the acknowledged flag
    // set.
    DCHECK(managed_user_shared_setting.acknowledged());
    dict->SetBooleanWithoutPathExpansion(
        kAcknowledged, managed_user_shared_setting.acknowledged());
    callbacks_.Notify(mu_id, key);

    seen_keys[mu_id].insert(key);
  }

  // Iterate over all settings that we have locally, which includes settings
  // that were just synced down. We filter those out using |seen_keys|.
  SyncChangeList change_list;
  const DictionaryValue* all_settings =
      prefs_->GetDictionary(prefs::kManagedUserSharedSettings);
  for (DictionaryValue::Iterator it(*all_settings); !it.IsAtEnd();
       it.Advance()) {
    const DictionaryValue* dict = NULL;
    bool success = it.value().GetAsDictionary(&dict);
    DCHECK(success);

    const std::set<std::string>& seen = seen_keys[it.key()];
    for (DictionaryValue::Iterator jt(*dict); !jt.IsAtEnd(); jt.Advance()) {
      // We only need to upload settings that we haven't seen in the initial
      // sync data (which means they were added locally).
      if (seen.count(jt.key()) > 0)
        continue;

      SyncData data = CreateSyncDataForValue(it.key(), jt.key(), jt.value());
      DCHECK(data.IsValid());
      change_list.push_back(
          SyncChange(FROM_HERE, SyncChange::ACTION_ADD, data));
    }
  }

  SyncMergeResult result(MANAGED_USER_SHARED_SETTINGS);
  // Process all the accumulated changes.
  if (change_list.size() > 0) {
    result.set_error(
        sync_processor_->ProcessSyncChanges(FROM_HERE, change_list));
  }

  // TODO(bauerb): Statistics?
  return result;
}

void ManagedUserSharedSettingsService::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(MANAGED_USER_SHARED_SETTINGS, type);
  sync_processor_.reset();
  error_handler_.reset();
}

syncer::SyncDataList ManagedUserSharedSettingsService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_EQ(MANAGED_USER_SHARED_SETTINGS, type);
  SyncDataList data;
  const DictionaryValue* all_settings =
      prefs_->GetDictionary(prefs::kManagedUserSharedSettings);
  for (DictionaryValue::Iterator it(*all_settings); !it.IsAtEnd();
       it.Advance()) {
    const DictionaryValue* dict = NULL;
    bool success = it.value().GetAsDictionary(&dict);
    DCHECK(success);
    for (DictionaryValue::Iterator jt(*dict); !jt.IsAtEnd(); jt.Advance()) {
      data.push_back(CreateSyncDataForValue(it.key(), jt.key(), jt.value()));
    }
  }
  return data;
}

syncer::SyncError ManagedUserSharedSettingsService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  for (SyncChangeList::const_iterator it = change_list.begin();
       it != change_list.end();
       ++it) {
    SyncData data = it->sync_data();
    DCHECK_EQ(MANAGED_USER_SHARED_SETTINGS, data.GetDataType());
    const ::sync_pb::ManagedUserSharedSettingSpecifics&
        managed_user_shared_setting =
            data.GetSpecifics().managed_user_shared_setting();
    const std::string& key = managed_user_shared_setting.key();
    const std::string& mu_id = managed_user_shared_setting.mu_id();
    ScopedManagedUserSharedSettingsUpdate update(prefs_, mu_id);
    DictionaryValue* update_dict = update.Get();
    DictionaryValue* dict = NULL;
    bool has_key = update_dict->GetDictionaryWithoutPathExpansion(key, &dict);
    switch (it->change_type()) {
      case SyncChange::ACTION_ADD:
      case SyncChange::ACTION_UPDATE: {
        // Every setting we get from the server should have the acknowledged
        // flag set.
        DCHECK(managed_user_shared_setting.acknowledged());

        if (has_key) {
          // If the managed user already exists, it should be an update action.
          DCHECK_EQ(SyncChange::ACTION_UPDATE, it->change_type());
        } else {
          // Otherwise, it should be an add action.
          DCHECK_EQ(SyncChange::ACTION_ADD, it->change_type());
          dict = new DictionaryValue;
          update_dict->SetWithoutPathExpansion(key, dict);
        }
        scoped_ptr<Value> value(
            base::JSONReader::Read(managed_user_shared_setting.value()));
        dict->SetWithoutPathExpansion(kValue, value.release());
        dict->SetBooleanWithoutPathExpansion(
            kAcknowledged, managed_user_shared_setting.acknowledged());
        break;
      }
      case SyncChange::ACTION_DELETE: {
        if (has_key)
          update_dict->RemoveWithoutPathExpansion(key, NULL);
        else
          NOTREACHED() << "Trying to delete nonexistent key " << key;
        break;
      }
      case SyncChange::ACTION_INVALID: {
        NOTREACHED();
        break;
      }
    }
    callbacks_.Notify(mu_id, key);
  }

  SyncError error;
  return error;
}
