// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_whitelist_service.h"

#include <string>

#include "base/files/file_path.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/values.h"
#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"
#include "chrome/browser/supervised_user/supervised_user_site_list.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_merge_result.h"
#include "sync/protocol/sync.pb.h"

const char kName[] = "name";

SupervisedUserWhitelistService::SupervisedUserWhitelistService(
    PrefService* prefs,
    scoped_ptr<component_updater::SupervisedUserWhitelistInstaller> installer)
    : prefs_(prefs),
      installer_(installer.Pass()),
      weak_ptr_factory_(this) {
  DCHECK(prefs);
  DCHECK(installer_);
}

SupervisedUserWhitelistService::~SupervisedUserWhitelistService() {
}

// static
void SupervisedUserWhitelistService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kSupervisedUserWhitelists,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void SupervisedUserWhitelistService::Init() {
  const base::DictionaryValue* whitelists =
      prefs_->GetDictionary(prefs::kSupervisedUserWhitelists);
  for (base::DictionaryValue::Iterator it(*whitelists); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* dict = nullptr;
    it.value().GetAsDictionary(&dict);
    std::string name;
    bool result = dict->GetString(kName, &name);
    DCHECK(result);
    bool new_installation = false;
    RegisterWhitelist(it.key(), name, new_installation);
  }
}

void SupervisedUserWhitelistService::AddSiteListsChangedCallback(
    const SiteListsChangedCallback& callback) {
  site_lists_changed_callbacks_.push_back(callback);
  NotifyWhitelistsChanged();
}

void SupervisedUserWhitelistService::LoadWhitelistForTesting(
    const std::string& id,
    const base::FilePath& path) {
  bool result = registered_whitelists_.insert(id).second;
  DCHECK(result);
  OnWhitelistReady(id, path);
}

void SupervisedUserWhitelistService::UnloadWhitelist(const std::string& id) {
  bool result = registered_whitelists_.erase(id) > 0u;
  DCHECK(result);
  loaded_whitelists_.erase(id);
  NotifyWhitelistsChanged();
}

// static
syncer::SyncData SupervisedUserWhitelistService::CreateWhitelistSyncData(
    const std::string& id,
    const std::string& name) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::ManagedUserWhitelistSpecifics* whitelist =
      specifics.mutable_managed_user_whitelist();
  whitelist->set_id(id);
  whitelist->set_name(name);

  return syncer::SyncData::CreateLocalData(id, name, specifics);
}

syncer::SyncMergeResult
SupervisedUserWhitelistService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK_EQ(syncer::SUPERVISED_USER_WHITELISTS, type);

  syncer::SyncChangeList change_list;
  syncer::SyncMergeResult result(syncer::SUPERVISED_USER_WHITELISTS);

  DictionaryPrefUpdate update(prefs_, prefs::kSupervisedUserWhitelists);
  base::DictionaryValue* pref_dict = update.Get();
  result.set_num_items_before_association(pref_dict->size());
  std::set<std::string> seen_ids;
  int num_items_added = 0;
  int num_items_modified = 0;

  for (const syncer::SyncData& sync_data : initial_sync_data) {
    DCHECK_EQ(syncer::SUPERVISED_USER_WHITELISTS, sync_data.GetDataType());
    const sync_pb::ManagedUserWhitelistSpecifics& whitelist =
        sync_data.GetSpecifics().managed_user_whitelist();
    std::string id = whitelist.id();
    std::string name = whitelist.name();
    seen_ids.insert(id);
    base::DictionaryValue* dict = nullptr;
    if (pref_dict->GetDictionary(id, &dict)) {
      std::string old_name;
      bool result = dict->GetString(kName, &old_name);
      DCHECK(result);
      if (name != old_name) {
        SetWhitelistProperties(dict, whitelist);
        num_items_modified++;
      }
    } else {
      num_items_added++;
      AddNewWhitelist(pref_dict, whitelist);
    }
  }

  std::set<std::string> ids_to_remove;
  for (base::DictionaryValue::Iterator it(*pref_dict); !it.IsAtEnd();
       it.Advance()) {
    if (seen_ids.find(it.key()) == seen_ids.end())
      ids_to_remove.insert(it.key());
  }

  for (const std::string& id : ids_to_remove)
    RemoveWhitelist(pref_dict, id);

  // Notify if whitelists have been uninstalled. We will notify about newly
  // added whitelists later, when they are actually available
  // (in OnWhitelistReady).
  if (!ids_to_remove.empty())
    NotifyWhitelistsChanged();

  result.set_num_items_added(num_items_added);
  result.set_num_items_modified(num_items_modified);
  result.set_num_items_deleted(ids_to_remove.size());
  result.set_num_items_after_association(pref_dict->size());
  return result;
}

void SupervisedUserWhitelistService::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(syncer::SUPERVISED_USER_WHITELISTS, type);
}

syncer::SyncDataList SupervisedUserWhitelistService::GetAllSyncData(
    syncer::ModelType type) const {
  syncer::SyncDataList sync_data;
  const base::DictionaryValue* whitelists =
      prefs_->GetDictionary(prefs::kSupervisedUserWhitelists);
  for (base::DictionaryValue::Iterator it(*whitelists); !it.IsAtEnd();
       it.Advance()) {
    const std::string& id = it.key();
    const base::DictionaryValue* dict = nullptr;
    it.value().GetAsDictionary(&dict);
    std::string name;
    bool result = dict->GetString(kName, &name);
    DCHECK(result);
    sync_pb::EntitySpecifics specifics;
    sync_pb::ManagedUserWhitelistSpecifics* whitelist =
        specifics.mutable_managed_user_whitelist();
    whitelist->set_id(id);
    whitelist->set_name(name);
    sync_data.push_back(syncer::SyncData::CreateLocalData(id, name, specifics));
  }
  return sync_data;
}

syncer::SyncError SupervisedUserWhitelistService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  bool whitelists_removed = false;
  syncer::SyncError error;
  DictionaryPrefUpdate update(prefs_, prefs::kSupervisedUserWhitelists);
  base::DictionaryValue* pref_dict = update.Get();
  for (const syncer::SyncChange& sync_change : change_list) {
    syncer::SyncData data = sync_change.sync_data();
    DCHECK_EQ(syncer::SUPERVISED_USER_WHITELISTS, data.GetDataType());
    const sync_pb::ManagedUserWhitelistSpecifics& whitelist =
        data.GetSpecifics().managed_user_whitelist();
    std::string id = whitelist.id();
    switch (sync_change.change_type()) {
      case syncer::SyncChange::ACTION_ADD: {
        DCHECK(!pref_dict->HasKey(id)) << id;
        AddNewWhitelist(pref_dict, whitelist);
        break;
      }
      case syncer::SyncChange::ACTION_UPDATE: {
        base::DictionaryValue* dict = nullptr;
        pref_dict->GetDictionaryWithoutPathExpansion(id, &dict);
        SetWhitelistProperties(dict, whitelist);
        break;
      }
      case syncer::SyncChange::ACTION_DELETE: {
        DCHECK(pref_dict->HasKey(id)) << id;
        RemoveWhitelist(pref_dict, id);
        whitelists_removed = true;
        break;
      }
      case syncer::SyncChange::ACTION_INVALID: {
        NOTREACHED();
        break;
      }
    }
  }

  if (whitelists_removed)
    NotifyWhitelistsChanged();

  return error;
}

void SupervisedUserWhitelistService::AddNewWhitelist(
    base::DictionaryValue* pref_dict,
    const sync_pb::ManagedUserWhitelistSpecifics& whitelist) {
  bool new_installation = true;
  RegisterWhitelist(whitelist.id(), whitelist.name(), new_installation);
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  SetWhitelistProperties(dict.get(), whitelist);
  pref_dict->SetWithoutPathExpansion(whitelist.id(), dict.release());
}

void SupervisedUserWhitelistService::SetWhitelistProperties(
    base::DictionaryValue* dict,
    const sync_pb::ManagedUserWhitelistSpecifics& whitelist) {
  dict->SetString(kName, whitelist.name());
}

void SupervisedUserWhitelistService::RemoveWhitelist(
    base::DictionaryValue* pref_dict,
    const std::string& id) {
  pref_dict->RemoveWithoutPathExpansion(id, NULL);
  installer_->UnregisterWhitelist(id);
  UnloadWhitelist(id);
}

void SupervisedUserWhitelistService::RegisterWhitelist(const std::string& id,
                                                       const std::string& name,
                                                       bool new_installation) {
  bool result = registered_whitelists_.insert(id).second;
  DCHECK(result);

  installer_->RegisterWhitelist(
      id, name, new_installation,
      base::Bind(&SupervisedUserWhitelistService::OnWhitelistReady,
                 weak_ptr_factory_.GetWeakPtr(), id));
}

void SupervisedUserWhitelistService::NotifyWhitelistsChanged() {
  for (const SiteListsChangedCallback& callback :
       site_lists_changed_callbacks_) {
    ScopedVector<SupervisedUserSiteList> whitelists;
    // TODO(bauerb): Load whitelists here and pass around immutable objects.
    for (const auto& whitelist : loaded_whitelists_) {
      whitelists.push_back(new SupervisedUserSiteList(whitelist.second));
    }
    callback.Run(whitelists.Pass());
  }
}

void SupervisedUserWhitelistService::OnWhitelistReady(
    const std::string& id,
    const base::FilePath& whitelist_path) {
  // If the whitelist has been unregistered in the mean time, ignore it.
  if (registered_whitelists_.count(id) == 0u)
    return;

  loaded_whitelists_[id] = whitelist_path;
  NotifyWhitelistsChanged();
}
