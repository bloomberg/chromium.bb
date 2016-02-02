// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_settings_service.h"

#include <stddef.h>
#include <utility>

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/common/chrome_constants.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
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
  return !base::StartsWith(name, kSupervisedUserInternalItemPrefix,
                           base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace

SupervisedUserSettingsService::SupervisedUserSettingsService(Profile* profile)
    : profile_(profile),
      active_(false),
      initialization_failed_(false),
      local_settings_(new base::DictionaryValue) {
}

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
  if (load_synchronously) {
    store_->ReadPrefs();
    // TODO(bauerb): Temporary CHECK while investigating
    // https://crbug.com/425785. Remove (or change to DCHECK) once the bug
    // is fixed.
    CHECK(store_->IsInitializationComplete());
  } else {
    store_->ReadPrefsAsync(NULL);
  }
}

void SupervisedUserSettingsService::Init(
    scoped_refptr<PersistentPrefStore> store) {
  DCHECK(!store_.get());
  store_ = store;
  store_->AddObserver(this);
}

scoped_ptr<SupervisedUserSettingsService::SettingsCallbackList::Subscription>
    SupervisedUserSettingsService::Subscribe(
    const SettingsCallback& callback) {
  if (IsReady()) {
    scoped_ptr<base::DictionaryValue> settings = GetSettings();
    callback.Run(settings.get());
  }

  return callback_list_.Add(callback);
}

Profile* SupervisedUserSettingsService::GetProfile(){
  return profile_;
}

void SupervisedUserSettingsService::SetActive(bool active) {
  active_ = active;
  InformSubscribers();
}

bool SupervisedUserSettingsService::IsReady() {
  // Initialization cannot be complete but have failed at the same time.
  DCHECK(!(store_->IsInitializationComplete() && initialization_failed_));
  return initialization_failed_ || store_->IsInitializationComplete();
}

void SupervisedUserSettingsService::Clear() {
  store_->RemoveValue(kAtomicSettings,
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  store_->RemoveValue(kSplitSettings,
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
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

void SupervisedUserSettingsService::SetLocalSetting(
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
  base::JSONWriter::Write(value, &json_value);
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
  sync_processor_ = std::move(sync_processor);
  error_handler_ = std::move(error_handler);

  // Clear all atomic and split settings, then recreate them from Sync data.
  Clear();
  for (const SyncData& sync_data : initial_sync_data) {
    DCHECK_EQ(SUPERVISED_USER_SETTINGS, sync_data.GetDataType());
    const ::sync_pb::ManagedUserSettingSpecifics& supervised_user_setting =
        sync_data.GetSpecifics().managed_user_setting();
    scoped_ptr<base::Value> value =
        JSONReader::Read(supervised_user_setting.value());
    std::string name_suffix = supervised_user_setting.name();
    base::DictionaryValue* dict = GetDictionaryAndSplitKey(&name_suffix);
    dict->SetWithoutPathExpansion(name_suffix, value.release());
  }
  store_->ReportValueChanged(kAtomicSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  store_->ReportValueChanged(kSplitSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
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
    store_->ReportValueChanged(kQueuedItems,
                               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
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
  for (const SyncChange& sync_change : change_list) {
    SyncData data = sync_change.sync_data();
    DCHECK_EQ(SUPERVISED_USER_SETTINGS, data.GetDataType());
    const ::sync_pb::ManagedUserSettingSpecifics& supervised_user_setting =
        data.GetSpecifics().managed_user_setting();
    std::string key = supervised_user_setting.name();
    base::DictionaryValue* dict = GetDictionaryAndSplitKey(&key);
    SyncChange::SyncChangeType change_type = sync_change.change_type();
    switch (change_type) {
      case SyncChange::ACTION_ADD:
      case SyncChange::ACTION_UPDATE: {
        scoped_ptr<base::Value> value =
            JSONReader::Read(supervised_user_setting.value());
        if (dict->HasKey(key)) {
          DLOG_IF(WARNING, change_type == SyncChange::ACTION_ADD)
              << "Value for key " << key << " already exists";
        } else {
          DLOG_IF(WARNING, change_type == SyncChange::ACTION_UPDATE)
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
  store_->ReportValueChanged(kAtomicSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  store_->ReportValueChanged(kSplitSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  InformSubscribers();

  SyncError error;
  return error;
}

void SupervisedUserSettingsService::OnPrefValueChanged(const std::string& key) {
}

void SupervisedUserSettingsService::OnInitializationCompleted(bool success) {
  if (!success) {
    // If this happens, it means the profile directory was not found. There is
    // not much we can do, but the whole profile will probably be useless
    // anyway. Just mark initialization as failed and continue otherwise,
    // because subscribers might still expect to be called back.
    initialization_failed_ = true;
  }

  // TODO(bauerb): Temporary CHECK while investigating https://crbug.com/425785.
  // Remove (or change back to DCHECK) once the bug is fixed.
  CHECK(IsReady());
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
    store_->SetValue(key, make_scoped_ptr(dict),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
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
  if (!active_ || initialization_failed_)
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

  return settings;
}

void SupervisedUserSettingsService::InformSubscribers() {
  if (!IsReady())
    return;

  scoped_ptr<base::DictionaryValue> settings = GetSettings();
  callback_list_.Notify(settings.get());
}
