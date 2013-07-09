// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/managed_mode_policy_provider.h"

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/prefs/json_pref_store.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "policy/policy_constants.h"
#include "sync/api/sync_change.h"
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

namespace {

const char kAtomicSettings[] = "atomic_settings";
const char kManagedUserInternalPolicyPrefix[] = "X-";
const char kQueuedItems[] = "queued_items";
const char kSplitSettingKeySeparator = ':';
const char kSplitSettings[] = "split_settings";

bool SettingShouldApplyToPolicy(const std::string& key) {
  return !StartsWithASCII(key, kManagedUserInternalPolicyPrefix, false);
}

}  // namespace

namespace policy {


// static
scoped_ptr<ManagedModePolicyProvider> ManagedModePolicyProvider::Create(
    Profile* profile,
    base::SequencedTaskRunner* sequenced_task_runner,
    bool force_load) {
  base::FilePath path =
      profile->GetPath().Append(chrome::kManagedModePolicyFilename);
  JsonPrefStore* pref_store = new JsonPrefStore(path, sequenced_task_runner);
  // Load the data synchronously if needed (when creating profiles on startup).
  if (force_load)
    pref_store->ReadPrefs();
  else
    pref_store->ReadPrefsAsync(NULL);

  return make_scoped_ptr(new ManagedModePolicyProvider(pref_store));
}

ManagedModePolicyProvider::ManagedModePolicyProvider(
    PersistentPrefStore* pref_store)
    : store_(pref_store),
      local_policies_(new DictionaryValue) {
  store_->AddObserver(this);
  if (store_->IsInitializationComplete())
    UpdatePolicyFromCache();
}

ManagedModePolicyProvider::~ManagedModePolicyProvider() {}

void ManagedModePolicyProvider::Clear() {
  store_->RemoveValue(kAtomicSettings);
  store_->RemoveValue(kSplitSettings);
}

// static
std::string ManagedModePolicyProvider::MakeSplitSettingKey(
    const std::string& prefix,
    const std::string& key) {
  return prefix + kSplitSettingKeySeparator + key;
}

void ManagedModePolicyProvider::UploadItem(const std::string& key,
                                           scoped_ptr<Value> value) {
  DCHECK(!SettingShouldApplyToPolicy(key));

  std::string key_suffix = key;
  DictionaryValue* dict = NULL;
  if (sync_processor_) {
    content::RecordAction(
        UserMetricsAction("ManagedUsers_UploadItem_Syncing"));
    dict = GetDictionaryAndSplitKey(&key_suffix);
    DCHECK(GetQueuedItems()->empty());
    SyncChangeList change_list;
    SyncData data = CreateSyncDataForPolicy(key, value.get());
    SyncChange::SyncChangeType change_type =
        dict->HasKey(key_suffix) ? SyncChange::ACTION_UPDATE
                                 : SyncChange::ACTION_ADD;
    change_list.push_back(SyncChange(FROM_HERE, change_type, data));
    SyncError error =
        sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
    DCHECK(!error.IsSet()) << error.ToString();
  } else {
    content::RecordAction(
        UserMetricsAction("ManagedUsers_UploadItem_Queued"));
    dict = GetQueuedItems();
  }
  dict->SetWithoutPathExpansion(key_suffix, value.release());
}

void ManagedModePolicyProvider::SetLocalPolicyForTesting(
    const std::string& key,
    scoped_ptr<Value> value) {
  if (value)
    local_policies_->SetWithoutPathExpansion(key, value.release());
  else
    local_policies_->RemoveWithoutPathExpansion(key, NULL);

  UpdatePolicyFromCache();
}

// static
SyncData ManagedModePolicyProvider::CreateSyncDataForPolicy(
    const std::string& name,
    const Value* value) {
  std::string json_value;
  base::JSONWriter::Write(value, &json_value);
  ::sync_pb::EntitySpecifics specifics;
  specifics.mutable_managed_user_setting()->set_name(name);
  specifics.mutable_managed_user_setting()->set_value(json_value);
  return SyncData::CreateLocalData(name, name, specifics);
}

void ManagedModePolicyProvider::Shutdown() {
  store_->RemoveObserver(this);
  ConfigurationPolicyProvider::Shutdown();
}

void ManagedModePolicyProvider::RefreshPolicies() {
  UpdatePolicyFromCache();
}

bool ManagedModePolicyProvider::IsInitializationComplete(
    PolicyDomain domain) const {
  if (domain == POLICY_DOMAIN_CHROME)
    return store_->IsInitializationComplete();
  return true;
}

void ManagedModePolicyProvider::OnPrefValueChanged(const std::string& key) {}

void ManagedModePolicyProvider::OnInitializationCompleted(bool success) {
  DCHECK(success);
  UpdatePolicyFromCache();
}

SyncMergeResult ManagedModePolicyProvider::MergeDataAndStartSyncing(
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
  UpdatePolicyFromCache();

  // Upload all the queued up items (either with an ADD or an UPDATE action,
  // depending on whether they already exist) and move them to split settings.
  SyncChangeList change_list;
  DictionaryValue* queued_items = GetQueuedItems();
  for (DictionaryValue::Iterator it(*queued_items); !it.IsAtEnd();
       it.Advance()) {
    std::string key_suffix = it.key();
    DictionaryValue* dict = GetDictionaryAndSplitKey(&key_suffix);
    SyncData data = CreateSyncDataForPolicy(it.key(), &it.value());
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

void ManagedModePolicyProvider::StopSyncing(ModelType type) {
  DCHECK_EQ(syncer::MANAGED_USER_SETTINGS, type);
  sync_processor_.reset();
  error_handler_.reset();
}

SyncDataList ManagedModePolicyProvider::GetAllSyncData(ModelType type) const {
  DCHECK_EQ(syncer::MANAGED_USER_SETTINGS, type);
  SyncDataList data;
  for (DictionaryValue::Iterator it(*GetAtomicSettings()); !it.IsAtEnd();
       it.Advance()) {
    data.push_back(CreateSyncDataForPolicy(it.key(), &it.value()));
  }
  for (DictionaryValue::Iterator it(*GetSplitSettings()); !it.IsAtEnd();
       it.Advance()) {
    const DictionaryValue* dict = NULL;
    it.value().GetAsDictionary(&dict);
    for (DictionaryValue::Iterator jt(*dict); !jt.IsAtEnd(); jt.Advance()) {
      data.push_back(CreateSyncDataForPolicy(
          MakeSplitSettingKey(it.key(), jt.key()), &jt.value()));
    }
  }
  DCHECK_EQ(0u, GetQueuedItems()->size());
  return data;
}

SyncError ManagedModePolicyProvider::ProcessSyncChanges(
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
  UpdatePolicyFromCache();

  SyncError error;
  return error;
}

void ManagedModePolicyProvider::InitLocalPolicies() {
  local_policies_->Clear();
  local_policies_->SetBoolean(policy::key::kAllowDeletingBrowserHistory, false);
  local_policies_->SetInteger(policy::key::kContentPackDefaultFilteringBehavior,
                              ManagedModeURLFilter::ALLOW);
  local_policies_->SetBoolean(policy::key::kForceSafeSearch, true);
  local_policies_->SetBoolean(policy::key::kHideWebStoreIcon, true);
  local_policies_->SetInteger(policy::key::kIncognitoModeAvailability,
                              IncognitoModePrefs::DISABLED);
  local_policies_->SetBoolean(policy::key::kSigninAllowed, false);
  UpdatePolicyFromCache();
}

DictionaryValue* ManagedModePolicyProvider::GetOrCreateDictionary(
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

DictionaryValue* ManagedModePolicyProvider::GetAtomicSettings() const {
  return GetOrCreateDictionary(kAtomicSettings);
}

DictionaryValue* ManagedModePolicyProvider::GetSplitSettings() const {
  return GetOrCreateDictionary(kSplitSettings);
}

DictionaryValue* ManagedModePolicyProvider::GetQueuedItems() const {
  return GetOrCreateDictionary(kQueuedItems);
}

DictionaryValue* ManagedModePolicyProvider::GetDictionaryAndSplitKey(
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

void ManagedModePolicyProvider::UpdatePolicyFromCache() {
  scoped_ptr<PolicyBundle> policy_bundle(new PolicyBundle);
  PolicyMap* policy_map = &policy_bundle->Get(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  policy_map->LoadFrom(
      local_policies_.get(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER);

  DictionaryValue* atomic_settings = GetAtomicSettings();
  for (DictionaryValue::Iterator it(*atomic_settings); !it.IsAtEnd();
       it.Advance()) {
    if (!SettingShouldApplyToPolicy(it.key()))
      continue;

    policy_map->Set(it.key(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                    it.value().DeepCopy(), NULL);
  }

  DictionaryValue* split_settings = GetSplitSettings();
  for (DictionaryValue::Iterator it(*split_settings); !it.IsAtEnd();
       it.Advance()) {
    if (!SettingShouldApplyToPolicy(it.key()))
      continue;

    policy_map->Set(it.key(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                    it.value().DeepCopy(), NULL);
  }
  UpdatePolicy(policy_bundle.Pass());
}

}  // namespace policy
