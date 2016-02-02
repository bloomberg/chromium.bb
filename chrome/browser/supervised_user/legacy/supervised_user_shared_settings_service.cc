// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service.h"

#include <map>
#include <set>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_merge_result.h"
#include "sync/protocol/sync.pb.h"

using base::DictionaryValue;
using base::Value;
using syncer::ModelType;
using syncer::SUPERVISED_USER_SHARED_SETTINGS;
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
  DictionaryValue* dict = nullptr;
  if (!parent->GetDictionaryWithoutPathExpansion(key, &dict)) {
    dict = new DictionaryValue;
    parent->SetWithoutPathExpansion(key, dict);
  }
  return dict;
}

class ScopedSupervisedUserSharedSettingsUpdate {
 public:
  ScopedSupervisedUserSharedSettingsUpdate(PrefService* prefs,
                                           const std::string& su_id)
      : update_(prefs, prefs::kSupervisedUserSharedSettings), su_id_(su_id) {
    DCHECK(!su_id.empty());

    // A supervised user can only modify their own settings.
    std::string id = prefs->GetString(prefs::kSupervisedUserId);
    DCHECK(id.empty() || id == su_id);
  }

  DictionaryValue* Get() {
    return FindOrCreateDictionary(update_.Get(), su_id_);
  }

 private:
  DictionaryPrefUpdate update_;
  std::string su_id_;
};

SyncData CreateSyncDataForValue(
    const std::string& su_id,
    const std::string& key,
    const Value& dict_value) {
  const DictionaryValue* dict = nullptr;
  if (!dict_value.GetAsDictionary(&dict))
    return SyncData();

  const Value* value = nullptr;
  if (!dict->Get(kValue, &value))
    return SyncData();

  bool acknowledged = false;
  dict->GetBoolean(kAcknowledged, &acknowledged);

  return SupervisedUserSharedSettingsService::CreateSyncDataForSetting(
      su_id, key, *value, acknowledged);
}

}  // namespace


SupervisedUserSharedSettingsService::SupervisedUserSharedSettingsService(
    PrefService* prefs)
    : prefs_(prefs) {}

SupervisedUserSharedSettingsService::~SupervisedUserSharedSettingsService() {}

void SupervisedUserSharedSettingsService::SetValueInternal(
    const std::string& su_id,
    const std::string& key,
    const Value& value,
    bool acknowledged) {
  ScopedSupervisedUserSharedSettingsUpdate update(prefs_, su_id);
  DictionaryValue* update_dict = update.Get();

  DictionaryValue* dict = nullptr;
  bool has_key = update_dict->GetDictionaryWithoutPathExpansion(key, &dict);
  if (!has_key) {
    dict = new DictionaryValue;
    update_dict->SetWithoutPathExpansion(key, dict);
  }
  dict->SetWithoutPathExpansion(kValue, value.DeepCopy());
  dict->SetBooleanWithoutPathExpansion(kAcknowledged, acknowledged);

  if (!sync_processor_)
    return;

  SyncData data = CreateSyncDataForSetting(su_id, key, value, acknowledged);
  SyncChange::SyncChangeType change_type =
      has_key ? SyncChange::ACTION_UPDATE : SyncChange::ACTION_ADD;
  SyncChangeList changes;
  changes.push_back(SyncChange(FROM_HERE, change_type, data));
  SyncError error = sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
  DCHECK(!error.IsSet()) << error.ToString();
}

const Value* SupervisedUserSharedSettingsService::GetValue(
    const std::string& su_id,
    const std::string& key) {
  const DictionaryValue* data =
      prefs_->GetDictionary(prefs::kSupervisedUserSharedSettings);
  const DictionaryValue* dict = nullptr;
  if (!data->GetDictionaryWithoutPathExpansion(su_id, &dict))
    return nullptr;

  const DictionaryValue* settings = nullptr;
  if (!dict->GetDictionaryWithoutPathExpansion(key, &settings))
    return nullptr;

  const Value* value = nullptr;
  if (!settings->GetWithoutPathExpansion(kValue, &value))
    return nullptr;

  return value;
}

void SupervisedUserSharedSettingsService::SetValue(
    const std::string& su_id,
    const std::string& key,
    const Value& value) {
  SetValueInternal(su_id, key, value, true);
}

scoped_ptr<
    SupervisedUserSharedSettingsService::ChangeCallbackList::Subscription>
SupervisedUserSharedSettingsService::Subscribe(
    const SupervisedUserSharedSettingsService::ChangeCallback& cb) {
  return callbacks_.Add(cb);
}

// static
void SupervisedUserSharedSettingsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kSupervisedUserSharedSettings);
}

// static
SyncData SupervisedUserSharedSettingsService::CreateSyncDataForSetting(
    const std::string& su_id,
    const std::string& key,
    const Value& value,
    bool acknowledged) {
  std::string json_value;
  base::JSONWriter::Write(value, &json_value);
  ::sync_pb::EntitySpecifics specifics;
  specifics.mutable_managed_user_shared_setting()->set_mu_id(su_id);
  specifics.mutable_managed_user_shared_setting()->set_key(key);
  specifics.mutable_managed_user_shared_setting()->set_value(json_value);
  specifics.mutable_managed_user_shared_setting()->set_acknowledged(
      acknowledged);
  std::string title = su_id + ":" + key;
  return SyncData::CreateLocalData(title, title, specifics);
}

void SupervisedUserSharedSettingsService::Shutdown() {}

syncer::SyncMergeResult
SupervisedUserSharedSettingsService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK_EQ(SUPERVISED_USER_SHARED_SETTINGS, type);
  sync_processor_ = std::move(sync_processor);
  error_handler_ = std::move(error_handler);

  int num_before_association = 0;
  std::map<std::string, std::set<std::string> > pref_seen_keys;
  const DictionaryValue* pref_dict =
      prefs_->GetDictionary(prefs::kSupervisedUserSharedSettings);
  for (DictionaryValue::Iterator it(*pref_dict); !it.IsAtEnd(); it.Advance()) {
    const DictionaryValue* dict = nullptr;
    bool success = it.value().GetAsDictionary(&dict);
    DCHECK(success);
    num_before_association += dict->size();
    for (DictionaryValue::Iterator jt(*dict); !jt.IsAtEnd(); jt.Advance())
      pref_seen_keys[it.key()].insert(jt.key());
  }

  // We keep a map from MU ID to the set of keys that we have seen in the
  // initial sync data.
  std::map<std::string, std::set<std::string> > sync_seen_keys;
  int num_added = 0;
  int num_modified = 0;

  // Iterate over all initial sync data, and update it locally. This means that
  // the value from the server always wins over a local value.
  for (const SyncData& sync_data : initial_sync_data) {
    DCHECK_EQ(SUPERVISED_USER_SHARED_SETTINGS, sync_data.GetDataType());
    const ::sync_pb::ManagedUserSharedSettingSpecifics&
        supervised_user_shared_setting =
            sync_data.GetSpecifics().managed_user_shared_setting();
    scoped_ptr<Value> value =
        base::JSONReader::Read(supervised_user_shared_setting.value());
    const std::string& su_id = supervised_user_shared_setting.mu_id();
    ScopedSupervisedUserSharedSettingsUpdate update(prefs_, su_id);
    const std::string& key = supervised_user_shared_setting.key();
    DictionaryValue* dict = FindOrCreateDictionary(update.Get(), key);
    dict->SetWithoutPathExpansion(kValue, value.release());

    // Every setting we get from the server should have the acknowledged flag
    // set.
    DCHECK(supervised_user_shared_setting.acknowledged());
    dict->SetBooleanWithoutPathExpansion(
        kAcknowledged, supervised_user_shared_setting.acknowledged());
    callbacks_.Notify(su_id, key);

    if (pref_seen_keys.find(su_id) == pref_seen_keys.end())
      num_added++;
    else
      num_modified++;

    sync_seen_keys[su_id].insert(key);
  }

  // Iterate over all settings that we have locally, which includes settings
  // that were just synced down. We filter those out using |sync_seen_keys|.
  SyncChangeList change_list;
  int num_after_association = 0;
  const DictionaryValue* all_settings =
      prefs_->GetDictionary(prefs::kSupervisedUserSharedSettings);
  for (DictionaryValue::Iterator it(*all_settings); !it.IsAtEnd();
       it.Advance()) {
    const DictionaryValue* dict = nullptr;
    bool success = it.value().GetAsDictionary(&dict);
    DCHECK(success);

    const std::set<std::string>& seen = sync_seen_keys[it.key()];
    num_after_association += dict->size();
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

  SyncMergeResult result(SUPERVISED_USER_SHARED_SETTINGS);
  // Process all the accumulated changes.
  if (change_list.size() > 0) {
    result.set_error(
        sync_processor_->ProcessSyncChanges(FROM_HERE, change_list));
  }

  result.set_num_items_added(num_added);
  result.set_num_items_modified(num_modified);
  result.set_num_items_before_association(num_before_association);
  result.set_num_items_after_association(num_after_association);
  return result;
}

void SupervisedUserSharedSettingsService::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(SUPERVISED_USER_SHARED_SETTINGS, type);
  sync_processor_.reset();
  error_handler_.reset();
}

syncer::SyncDataList SupervisedUserSharedSettingsService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_EQ(SUPERVISED_USER_SHARED_SETTINGS, type);
  SyncDataList data;
  const DictionaryValue* all_settings =
      prefs_->GetDictionary(prefs::kSupervisedUserSharedSettings);
  for (DictionaryValue::Iterator it(*all_settings); !it.IsAtEnd();
       it.Advance()) {
    const DictionaryValue* dict = nullptr;
    bool success = it.value().GetAsDictionary(&dict);
    DCHECK(success);
    for (DictionaryValue::Iterator jt(*dict); !jt.IsAtEnd(); jt.Advance()) {
      data.push_back(CreateSyncDataForValue(it.key(), jt.key(), jt.value()));
    }
  }
  return data;
}

syncer::SyncError SupervisedUserSharedSettingsService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  for (const SyncChange& sync_change : change_list) {
    SyncData data = sync_change.sync_data();
    DCHECK_EQ(SUPERVISED_USER_SHARED_SETTINGS, data.GetDataType());
    const ::sync_pb::ManagedUserSharedSettingSpecifics&
        supervised_user_shared_setting =
            data.GetSpecifics().managed_user_shared_setting();
    const std::string& key = supervised_user_shared_setting.key();
    const std::string& su_id = supervised_user_shared_setting.mu_id();
    ScopedSupervisedUserSharedSettingsUpdate update(prefs_, su_id);
    DictionaryValue* update_dict = update.Get();
    DictionaryValue* dict = nullptr;
    bool has_key = update_dict->GetDictionaryWithoutPathExpansion(key, &dict);
    switch (sync_change.change_type()) {
      case SyncChange::ACTION_ADD:
      case SyncChange::ACTION_UPDATE: {
        // Every setting we get from the server should have the acknowledged
        // flag set.
        DCHECK(supervised_user_shared_setting.acknowledged());

        if (has_key) {
          // If the supervised user already exists, it should be an update
          // action.
          DCHECK_EQ(SyncChange::ACTION_UPDATE, sync_change.change_type());
        } else {
          // Otherwise, it should be an add action.
          DCHECK_EQ(SyncChange::ACTION_ADD, sync_change.change_type());
          dict = new DictionaryValue;
          update_dict->SetWithoutPathExpansion(key, dict);
        }
        scoped_ptr<Value> value =
            base::JSONReader::Read(supervised_user_shared_setting.value());
        dict->SetWithoutPathExpansion(kValue, value.release());
        dict->SetBooleanWithoutPathExpansion(
            kAcknowledged, supervised_user_shared_setting.acknowledged());
        break;
      }
      case SyncChange::ACTION_DELETE: {
        if (has_key)
          update_dict->RemoveWithoutPathExpansion(key, nullptr);
        else
          NOTREACHED() << "Trying to delete nonexistent key " << key;
        break;
      }
      case SyncChange::ACTION_INVALID: {
        NOTREACHED();
        break;
      }
    }
    callbacks_.Notify(su_id, key);
  }

  SyncError error;
  return error;
}
