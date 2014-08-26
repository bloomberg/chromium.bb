// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_settings_service.h"

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_filter.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/sync.pb.h"

using base::DictionaryValue;
using base::JSONReader;
using base::UserMetricsAction;
using base::Value;
using content::BrowserThread;
using syncer::SUPERVISED_USER_SETTINGS;
using syncer::ModelType;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncChangeProcessor;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using syncer::SyncErrorFactory;
using syncer::SyncMergeResult;

const char kAtomicSettings[] = "atomic_settings";
const char kSupervisedUserInternalItemPrefix[] = "X-";
const char kQueuedItems[] = "queued_items";
const char kSplitSettingKeySeparator = ':';
const char kSplitSettings[] = "split_settings";

namespace {

bool SettingShouldApplyToPrefs(const std::string& name) {
  return !StartsWithASCII(name, kSupervisedUserInternalItemPrefix, false);
}

}  // namespace

SupervisedUserSettingsService::SupervisedUserSettingsService()
    : active_(false), local_settings_(new base::DictionaryValue) {}

SupervisedUserSettingsService::~SupervisedUserSettingsService() {}

void SupervisedUserSettingsService::Init(
    base::FilePath profile_path,
    base::SequencedTaskRunner* sequenced_task_runner,
    bool load_synchronously) {
  base::FilePath path =
      profile_path.Append(chrome::kSupervisedUserSettingsFilename);
  PersistentPrefStore* store = new JsonPrefStore(
      path, sequenced_task_runner, scoped_ptr<PrefFilter>());
  Init(store);
  if (load_synchronously)
    store_->ReadPrefs();
  else
    store_->ReadPrefsAsync(NULL);
}

void SupervisedUserSettingsService::Init(
    scoped_refptr<PersistentPrefStore> store) {
  DCHECK(!store_.get());
  store_ = store;
  store_->AddObserver(this);
}

void SupervisedUserSettingsService::Subscribe(
    const SettingsCallback& callback) {
  if (IsReady()) {
    scoped_ptr<base::DictionaryValue> settings = GetSettings();
    callback.Run(settings.get());
  }

  subscribers_.push_back(callback);
}

void SupervisedUserSettingsService::SetActive(bool active) {
  active_ = active;
  InformSubscribers();
}

bool SupervisedUserSettingsService::IsReady() {
  return store_->IsInitializationComplete();
}

void SupervisedUserSettingsService::Clear() {
  store_->RemoveValue(kAtomicSettings);
  store_->RemoveValue(kSplitSettings);
}

// static
std::string SupervisedUserSettingsService::MakeSplitSettingKey(
    const std::string& prefix,
    const std::string& key) {
  return prefix + kSplitSettingKeySeparator + key;
}

void SupervisedUserSettingsService::UploadItem(const std::string& key,
                                               scoped_ptr<base::Value> value) {
  DCHECK(!SettingShouldApplyToPrefs(key));

  std::string key_suffix = key;
  base::DictionaryValue* dict = NULL;
  if (sync_processor_) {
    content::RecordAction(UserMetricsAction("ManagedUsers_UploadItem_Syncing"));
    dict = GetDictionaryAndSplitKey(&key_suffix);
    DCHECK(GetQueuedItems()->empty());
    SyncChangeList change_list;
    SyncData data = CreateSyncDataForSetting(key, *value);
    SyncChange::SyncChangeType change_type =
        dict->HasKey(key_suffix) ? SyncChange::ACTION_UPDATE
                                 : SyncChange::ACTION_ADD;
    change_list.push_back(SyncChange(FROM_HERE, change_type, data));
    SyncError error =
        sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
    DCHECK(!error.IsSet()) << error.ToString();
  } else {
    // Queue the item up to be uploaded when we start syncing
    // (in MergeDataAndStartSyncing()).
    content::RecordAction(UserMetricsAction("ManagedUsers_UploadItem_Queued"));
    dict = GetQueuedItems();
  }
  dict->SetWithoutPathExpansion(key_suffix, value.release());
}

void SupervisedUserSettingsService::SetLocalSettingForTesting(
    const std::string& key,
    scoped_ptr<base::Value> value) {
  if (value)
    local_settings_->SetWithoutPathExpansion(key, value.release());
  else
    local_settings_->RemoveWithoutPathExpansion(key, NULL);

  InformSubscribers();
}

// static
SyncData SupervisedUserSettingsService::CreateSyncDataForSetting(
    const std::string& name,
    const base::Value& value) {
  std::string json_value;
  base::JSONWriter::Write(&value, &json_value);
  ::sync_pb::EntitySpecifics specifics;
  specifics.mutable_managed_user_setting()->set_name(name);
  specifics.mutable_managed_user_setting()->set_value(json_value);
  return SyncData::CreateLocalData(name, name, specifics);
}

void SupervisedUserSettingsService::Shutdown() {
  store_->RemoveObserver(this);
}

SyncMergeResult SupervisedUserSettingsService::MergeDataAndStartSyncing(
    ModelType type,
    const SyncDataList& initial_sync_data,
    scoped_ptr<SyncChangeProcessor> sync_processor,
    scoped_ptr<SyncErrorFactory> error_handler) {
  DCHECK_EQ(SUPERVISED_USER_SETTINGS, type);
  sync_processor_ = sync_processor.Pass();
  error_handler_ = error_handler.Pass();

  // Clear all atomic and split settings, then recreate them from Sync data.
  Clear();
  for (SyncDataList::const_iterator it = initial_sync_data.begin();
       it != initial_sync_data.end(); ++it) {
    DCHECK_EQ(SUPERVISED_USER_SETTINGS, it->GetDataType());
    const ::sync_pb::ManagedUserSettingSpecifics& supervised_user_setting =
        it->GetSpecifics().managed_user_setting();
    scoped_ptr<base::Value> value(
        JSONReader::Read(supervised_user_setting.value()));
    std::string name_suffix = supervised_user_setting.name();
    base::DictionaryValue* dict = GetDictionaryAndSplitKey(&name_suffix);
    dict->SetWithoutPathExpansion(name_suffix, value.release());
  }
  store_->ReportValueChanged(kAtomicSettings);
  store_->ReportValueChanged(kSplitSettings);
  InformSubscribers();

  // Upload all the queued up items (either with an ADD or an UPDATE action,
  // depending on whether they already exist) and move them to split settings.
  SyncChangeList change_list;
  base::DictionaryValue* queued_items = GetQueuedItems();
  for (base::DictionaryValue::Iterator it(*queued_items); !it.IsAtEnd();
       it.Advance()) {
    std::string key_suffix = it.key();
    base::DictionaryValue* dict = GetDictionaryAndSplitKey(&key_suffix);
    SyncData data = CreateSyncDataForSetting(it.key(), it.value());
    SyncChange::SyncChangeType change_type =
        dict->HasKey(key_suffix) ? SyncChange::ACTION_UPDATE
                                 : SyncChange::ACTION_ADD;
    change_list.push_back(SyncChange(FROM_HERE, change_type, data));
    dict->SetWithoutPathExpansion(key_suffix, it.value().DeepCopy());
  }
  queued_items->Clear();

  SyncMergeResult result(SUPERVISED_USER_SETTINGS);
  // Process all the accumulated changes from the queued items.
  if (change_list.size() > 0) {
    store_->ReportValueChanged(kQueuedItems);
    result.set_error(
        sync_processor_->ProcessSyncChanges(FROM_HERE, change_list));
  }

  // TODO(bauerb): Statistics?
  return result;
}

void SupervisedUserSettingsService::StopSyncing(ModelType type) {
  DCHECK_EQ(syncer::SUPERVISED_USER_SETTINGS, type);
  sync_processor_.reset();
  error_handler_.reset();
}

SyncDataList SupervisedUserSettingsService::GetAllSyncData(
    ModelType type) const {
  DCHECK_EQ(syncer::SUPERVISED_USER_SETTINGS, type);
  SyncDataList data;
  for (base::DictionaryValue::Iterator it(*GetAtomicSettings()); !it.IsAtEnd();
       it.Advance()) {
    data.push_back(CreateSyncDataForSetting(it.key(), it.value()));
  }
  for (base::DictionaryValue::Iterator it(*GetSplitSettings()); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* dict = NULL;
    it.value().GetAsDictionary(&dict);
    for (base::DictionaryValue::Iterator jt(*dict);
         !jt.IsAtEnd(); jt.Advance()) {
      data.push_back(CreateSyncDataForSetting(
          MakeSplitSettingKey(it.key(), jt.key()), jt.value()));
    }
  }
  DCHECK_EQ(0u, GetQueuedItems()->size());
  return data;
}

SyncError SupervisedUserSettingsService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  for (SyncChangeList::const_iterator it = change_list.begin();
       it != change_list.end(); ++it) {
    SyncData data = it->sync_data();
    DCHECK_EQ(SUPERVISED_USER_SETTINGS, data.GetDataType());
    const ::sync_pb::ManagedUserSettingSpecifics& supervised_user_setting =
        data.GetSpecifics().managed_user_setting();
    std::string key = supervised_user_setting.name();
    base::DictionaryValue* dict = GetDictionaryAndSplitKey(&key);
    switch (it->change_type()) {
      case SyncChange::ACTION_ADD:
      case SyncChange::ACTION_UPDATE: {
        scoped_ptr<base::Value> value(
            JSONReader::Read(supervised_user_setting.value()));
        if (dict->HasKey(key)) {
          DLOG_IF(WARNING, it->change_type() == SyncChange::ACTION_ADD)
              << "Value for key " << key << " already exists";
        } else {
          DLOG_IF(WARNING, it->change_type() == SyncChange::ACTION_UPDATE)
              << "Value for key " << key << " doesn't exist yet";
        }
        dict->SetWithoutPathExpansion(key, value.release());
        break;
      }
      case SyncChange::ACTION_DELETE: {
        DLOG_IF(WARNING, !dict->HasKey(key)) << "Trying to delete nonexistent "
                                             << "key " << key;
        dict->RemoveWithoutPathExpansion(key, NULL);
        break;
      }
      case SyncChange::ACTION_INVALID: {
        NOTREACHED();
        break;
      }
    }
  }
  store_->ReportValueChanged(kAtomicSettings);
  store_->ReportValueChanged(kSplitSettings);
  InformSubscribers();

  SyncError error;
  return error;
}

void SupervisedUserSettingsService::OnPrefValueChanged(const std::string& key) {
}

void SupervisedUserSettingsService::OnInitializationCompleted(bool success) {
  DCHECK(success);
  DCHECK(IsReady());
  InformSubscribers();
}

base::DictionaryValue* SupervisedUserSettingsService::GetOrCreateDictionary(
    const std::string& key) const {
  base::Value* value = NULL;
  base::DictionaryValue* dict = NULL;
  if (store_->GetMutableValue(key, &value)) {
    bool success = value->GetAsDictionary(&dict);
    DCHECK(success);
  } else {
    dict = new base::DictionaryValue;
    store_->SetValue(key, dict);
  }

  return dict;
}

base::DictionaryValue*
SupervisedUserSettingsService::GetAtomicSettings() const {
  return GetOrCreateDictionary(kAtomicSettings);
}

base::DictionaryValue* SupervisedUserSettingsService::GetSplitSettings() const {
  return GetOrCreateDictionary(kSplitSettings);
}

base::DictionaryValue* SupervisedUserSettingsService::GetQueuedItems() const {
  return GetOrCreateDictionary(kQueuedItems);
}

base::DictionaryValue* SupervisedUserSettingsService::GetDictionaryAndSplitKey(
    std::string* key) const {
  size_t pos = key->find_first_of(kSplitSettingKeySeparator);
  if (pos == std::string::npos)
    return GetAtomicSettings();

  base::DictionaryValue* split_settings = GetSplitSettings();
  std::string prefix = key->substr(0, pos);
  base::DictionaryValue* dict = NULL;
  if (!split_settings->GetDictionary(prefix, &dict)) {
    dict = new base::DictionaryValue;
    DCHECK(!split_settings->HasKey(prefix));
    split_settings->Set(prefix, dict);
  }
  key->erase(0, pos + 1);
  return dict;
}

scoped_ptr<base::DictionaryValue> SupervisedUserSettingsService::GetSettings() {
  DCHECK(IsReady());
  if (!active_)
    return scoped_ptr<base::DictionaryValue>();

  scoped_ptr<base::DictionaryValue> settings(local_settings_->DeepCopy());

  base::DictionaryValue* atomic_settings = GetAtomicSettings();
  for (base::DictionaryValue::Iterator it(*atomic_settings); !it.IsAtEnd();
       it.Advance()) {
    if (!SettingShouldApplyToPrefs(it.key()))
      continue;

    settings->Set(it.key(), it.value().DeepCopy());
  }

  base::DictionaryValue* split_settings = GetSplitSettings();
  for (base::DictionaryValue::Iterator it(*split_settings); !it.IsAtEnd();
       it.Advance()) {
    if (!SettingShouldApplyToPrefs(it.key()))
      continue;

    settings->Set(it.key(), it.value().DeepCopy());
  }

  return settings.Pass();
}

void SupervisedUserSettingsService::InformSubscribers() {
  if (!IsReady())
    return;

  scoped_ptr<base::DictionaryValue> settings = GetSettings();
  for (std::vector<SettingsCallback>::iterator it = subscribers_.begin();
       it != subscribers_.end(); ++it) {
    it->Run(settings.get());
  }
}
