// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_settings_service.h"

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/prefs/json_pref_store.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/sync.pb.h"

using base::DictionaryValue;
using base::JSONReader;
using base::Value;
using content::BrowserThread;
using content::UserMetricsAction;
using syncer::MANAGED_USER_SETTINGS;
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
const char kManagedUserInternalItemPrefix[] = "X-";
const char kQueuedItems[] = "queued_items";
const char kSplitSettingKeySeparator = ':';
const char kSplitSettings[] = "split_settings";

namespace {

bool SettingShouldApplyToPrefs(const std::string& name) {
  return !StartsWithASCII(name, kManagedUserInternalItemPrefix, false);
}

}  // namespace

ManagedUserSettingsService::ManagedUserSettingsService()
    : active_(false), local_settings_(new DictionaryValue) {}

ManagedUserSettingsService::~ManagedUserSettingsService() {}

void ManagedUserSettingsService::Init(
    base::FilePath profile_path,
    base::SequencedTaskRunner* sequenced_task_runner,
    bool load_synchronously) {
  base::FilePath path =
      profile_path.Append(chrome::kManagedUserSettingsFilename);
  PersistentPrefStore* store = new JsonPrefStore(path, sequenced_task_runner);
  Init(store);
  if (load_synchronously)
    store_->ReadPrefs();
  else
    store_->ReadPrefsAsync(NULL);
}

void ManagedUserSettingsService::Init(
    scoped_refptr<PersistentPrefStore> store) {
  DCHECK(!store_);
  store_ = store;
  store_->AddObserver(this);
}

void ManagedUserSettingsService::Subscribe(const SettingsCallback& callback) {
  if (IsReady()) {
    scoped_ptr<base::DictionaryValue> settings = GetSettings();
    callback.Run(settings.get());
  }

  subscribers_.push_back(callback);
}

void ManagedUserSettingsService::Activate() {
  active_ = true;
  InformSubscribers();
}

bool ManagedUserSettingsService::IsReady() {
  return store_->IsInitializationComplete();
}

void ManagedUserSettingsService::Clear() {
  store_->RemoveValue(kAtomicSettings);
  store_->RemoveValue(kSplitSettings);
}

// static
std::string ManagedUserSettingsService::MakeSplitSettingKey(
    const std::string& prefix,
    const std::string& key) {
  return prefix + kSplitSettingKeySeparator + key;
}

void ManagedUserSettingsService::UploadItem(const std::string& key,
                                                scoped_ptr<Value> value) {
  DCHECK(!SettingShouldApplyToPrefs(key));

  std::string key_suffix = key;
  DictionaryValue* dict = NULL;
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

void ManagedUserSettingsService::SetLocalSettingForTesting(
    const std::string& key,
    scoped_ptr<Value> value) {
  if (value)
    local_settings_->SetWithoutPathExpansion(key, value.release());
  else
    local_settings_->RemoveWithoutPathExpansion(key, NULL);

  InformSubscribers();
}

// static
SyncData ManagedUserSettingsService::CreateSyncDataForSetting(
    const std::string& name,
    const Value& value) {
  std::string json_value;
  base::JSONWriter::Write(&value, &json_value);
  ::sync_pb::EntitySpecifics specifics;
  specifics.mutable_managed_user_setting()->set_name(name);
  specifics.mutable_managed_user_setting()->set_value(json_value);
  return SyncData::CreateLocalData(name, name, specifics);
}

void ManagedUserSettingsService::Shutdown() {
  store_->RemoveObserver(this);
}

SyncMergeResult ManagedUserSettingsService::MergeDataAndStartSyncing(
    ModelType type,
    const SyncDataList& initial_sync_data,
    scoped_ptr<SyncChangeProcessor> sync_processor,
    scoped_ptr<SyncErrorFactory> error_handler) {
  DCHECK_EQ(MANAGED_USER_SETTINGS, type);
  sync_processor_ = sync_processor.Pass();
  error_handler_ = error_handler.Pass();

  // Clear all atomic and split settings, then recreate them from Sync data.
  Clear();
  for (SyncDataList::const_iterator it = initial_sync_data.begin();
       it != initial_sync_data.end(); ++it) {
    DCHECK_EQ(MANAGED_USER_SETTINGS, it->GetDataType());
    const ::sync_pb::ManagedUserSettingSpecifics& managed_user_setting =
        it->GetSpecifics().managed_user_setting();
    scoped_ptr<Value> value(JSONReader::Read(managed_user_setting.value()));
    std::string name_suffix = managed_user_setting.name();
    DictionaryValue* dict = GetDictionaryAndSplitKey(&name_suffix);
    dict->SetWithoutPathExpansion(name_suffix, value.release());
  }
  store_->ReportValueChanged(kAtomicSettings);
  store_->ReportValueChanged(kSplitSettings);
  InformSubscribers();

  // Upload all the queued up items (either with an ADD or an UPDATE action,
  // depending on whether they already exist) and move them to split settings.
  SyncChangeList change_list;
  DictionaryValue* queued_items = GetQueuedItems();
  for (DictionaryValue::Iterator it(*queued_items); !it.IsAtEnd();
       it.Advance()) {
    std::string key_suffix = it.key();
    DictionaryValue* dict = GetDictionaryAndSplitKey(&key_suffix);
    SyncData data = CreateSyncDataForSetting(it.key(), it.value());
    SyncChange::SyncChangeType change_type =
        dict->HasKey(key_suffix) ? SyncChange::ACTION_UPDATE
                                 : SyncChange::ACTION_ADD;
    change_list.push_back(SyncChange(FROM_HERE, change_type, data));
    dict->SetWithoutPathExpansion(key_suffix, it.value().DeepCopy());
  }
  queued_items->Clear();

  SyncMergeResult result(MANAGED_USER_SETTINGS);
  // Process all the accumulated changes from the queued items.
  if (change_list.size() > 0) {
    store_->ReportValueChanged(kQueuedItems);
    result.set_error(
        sync_processor_->ProcessSyncChanges(FROM_HERE, change_list));
  }

  // TODO(bauerb): Statistics?
  return result;
}

void ManagedUserSettingsService::StopSyncing(ModelType type) {
  DCHECK_EQ(syncer::MANAGED_USER_SETTINGS, type);
  sync_processor_.reset();
  error_handler_.reset();
}

SyncDataList ManagedUserSettingsService::GetAllSyncData(
    ModelType type) const {
  DCHECK_EQ(syncer::MANAGED_USER_SETTINGS, type);
  SyncDataList data;
  for (DictionaryValue::Iterator it(*GetAtomicSettings()); !it.IsAtEnd();
       it.Advance()) {
    data.push_back(CreateSyncDataForSetting(it.key(), it.value()));
  }
  for (DictionaryValue::Iterator it(*GetSplitSettings()); !it.IsAtEnd();
       it.Advance()) {
    const DictionaryValue* dict = NULL;
    it.value().GetAsDictionary(&dict);
    for (DictionaryValue::Iterator jt(*dict); !jt.IsAtEnd(); jt.Advance()) {
      data.push_back(CreateSyncDataForSetting(
          MakeSplitSettingKey(it.key(), jt.key()), jt.value()));
    }
  }
  DCHECK_EQ(0u, GetQueuedItems()->size());
  return data;
}

SyncError ManagedUserSettingsService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  for (SyncChangeList::const_iterator it = change_list.begin();
       it != change_list.end(); ++it) {
    SyncData data = it->sync_data();
    DCHECK_EQ(MANAGED_USER_SETTINGS, data.GetDataType());
    const ::sync_pb::ManagedUserSettingSpecifics& managed_user_setting =
        data.GetSpecifics().managed_user_setting();
    std::string key = managed_user_setting.name();
    DictionaryValue* dict = GetDictionaryAndSplitKey(&key);
    switch (it->change_type()) {
      case SyncChange::ACTION_ADD:
      case SyncChange::ACTION_UPDATE: {
        scoped_ptr<Value> value(JSONReader::Read(managed_user_setting.value()));
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

void ManagedUserSettingsService::OnPrefValueChanged(const std::string& key) {}

void ManagedUserSettingsService::OnInitializationCompleted(bool success) {
  DCHECK(success);
  DCHECK(IsReady());
  InformSubscribers();
}

DictionaryValue* ManagedUserSettingsService::GetOrCreateDictionary(
    const std::string& key) const {
  Value* value = NULL;
  DictionaryValue* dict = NULL;
  if (store_->GetMutableValue(key, &value)) {
    bool success = value->GetAsDictionary(&dict);
    DCHECK(success);
  } else {
    dict = new base::DictionaryValue;
    store_->SetValue(key, dict);
  }

  return dict;
}

DictionaryValue* ManagedUserSettingsService::GetAtomicSettings() const {
  return GetOrCreateDictionary(kAtomicSettings);
}

DictionaryValue* ManagedUserSettingsService::GetSplitSettings() const {
  return GetOrCreateDictionary(kSplitSettings);
}

DictionaryValue* ManagedUserSettingsService::GetQueuedItems() const {
  return GetOrCreateDictionary(kQueuedItems);
}

DictionaryValue* ManagedUserSettingsService::GetDictionaryAndSplitKey(
    std::string* key) const {
  size_t pos = key->find_first_of(kSplitSettingKeySeparator);
  if (pos == std::string::npos)
    return GetAtomicSettings();

  DictionaryValue* split_settings = GetSplitSettings();
  std::string prefix = key->substr(0, pos);
  DictionaryValue* dict = NULL;
  if (!split_settings->GetDictionary(prefix, &dict)) {
    dict = new DictionaryValue;
    DCHECK(!split_settings->HasKey(prefix));
    split_settings->Set(prefix, dict);
  }
  key->erase(0, pos + 1);
  return dict;
}

scoped_ptr<DictionaryValue> ManagedUserSettingsService::GetSettings() {
  DCHECK(IsReady());
  if (!active_)
    return scoped_ptr<base::DictionaryValue>();

  scoped_ptr<DictionaryValue> settings(local_settings_->DeepCopy());

  DictionaryValue* atomic_settings = GetAtomicSettings();
  for (DictionaryValue::Iterator it(*atomic_settings); !it.IsAtEnd();
       it.Advance()) {
    if (!SettingShouldApplyToPrefs(it.key()))
      continue;

    settings->Set(it.key(), it.value().DeepCopy());
  }

  DictionaryValue* split_settings = GetSplitSettings();
  for (DictionaryValue::Iterator it(*split_settings); !it.IsAtEnd();
       it.Advance()) {
    if (!SettingShouldApplyToPrefs(it.key()))
      continue;

    settings->Set(it.key(), it.value().DeepCopy());
  }

  return settings.Pass();
}

void ManagedUserSettingsService::InformSubscribers() {
  if (!IsReady())
    return;

  scoped_ptr<base::DictionaryValue> settings = GetSettings();
  for (std::vector<SettingsCallback>::iterator it = subscribers_.begin();
       it != subscribers_.end(); ++it) {
    it->Run(settings.get());
  }
}
