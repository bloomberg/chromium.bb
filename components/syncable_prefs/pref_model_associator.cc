// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/syncable_prefs/pref_model_associator.h"

#include "base/auto_reset.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/syncable_prefs/pref_model_associator_client.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/preference_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

using syncer::PREFERENCES;
using syncer::PRIORITY_PREFERENCES;

namespace syncable_prefs {

namespace {

const sync_pb::PreferenceSpecifics& GetSpecifics(const syncer::SyncData& pref) {
  DCHECK(pref.GetDataType() == syncer::PREFERENCES ||
         pref.GetDataType() == syncer::PRIORITY_PREFERENCES);
  if (pref.GetDataType() == syncer::PRIORITY_PREFERENCES) {
    return pref.GetSpecifics().priority_preference().preference();
  } else {
    return pref.GetSpecifics().preference();
  }
}

sync_pb::PreferenceSpecifics* GetMutableSpecifics(
    const syncer::ModelType type,
    sync_pb::EntitySpecifics* specifics) {
  if (type == syncer::PRIORITY_PREFERENCES) {
    DCHECK(!specifics->has_preference());
    return specifics->mutable_priority_preference()->mutable_preference();
  } else {
    DCHECK(!specifics->has_priority_preference());
    return specifics->mutable_preference();
  }
}

}  // namespace

PrefModelAssociator::PrefModelAssociator(
    const PrefModelAssociatorClient* client,
    syncer::ModelType type)
    : models_associated_(false),
      processing_syncer_changes_(false),
      pref_service_(NULL),
      type_(type),
      client_(client) {
  DCHECK(CalledOnValidThread());
  DCHECK(type_ == PREFERENCES || type_ == PRIORITY_PREFERENCES);
}

PrefModelAssociator::~PrefModelAssociator() {
  DCHECK(CalledOnValidThread());
  pref_service_ = NULL;

  STLDeleteContainerPairSecondPointers(synced_pref_observers_.begin(),
                                       synced_pref_observers_.end());
  synced_pref_observers_.clear();
}

void PrefModelAssociator::InitPrefAndAssociate(
    const syncer::SyncData& sync_pref,
    const std::string& pref_name,
    syncer::SyncChangeList* sync_changes,
    SyncDataMap* migrated_preference_list) {
  const base::Value* user_pref_value = pref_service_->GetUserPrefValue(
      pref_name.c_str());
  VLOG(1) << "Associating preference " << pref_name;

  if (sync_pref.IsValid()) {
    const sync_pb::PreferenceSpecifics& preference = GetSpecifics(sync_pref);
    std::string old_pref_name;
    DCHECK(pref_name == preference.name() ||
           (client_ &&
            client_->IsMigratedPreference(pref_name, &old_pref_name) &&
            preference.name() == old_pref_name));
    base::JSONReader reader;
    scoped_ptr<base::Value> sync_value(reader.ReadToValue(preference.value()));
    if (!sync_value.get()) {
      LOG(ERROR) << "Failed to deserialize preference value: "
                 << reader.GetErrorMessage();
      return;
    }

    if (user_pref_value) {
      DVLOG(1) << "Found user pref value for " << pref_name;
      // We have both server and local values. Merge them.
      scoped_ptr<base::Value> new_value(
          MergePreference(pref_name, *user_pref_value, *sync_value));

      // Update the local preference based on what we got from the
      // sync server. Note: this only updates the user value store, which is
      // ignored if the preference is policy controlled.
      if (new_value->IsType(base::Value::TYPE_NULL)) {
        LOG(WARNING) << "Sync has null value for pref " << pref_name.c_str();
        pref_service_->ClearPref(pref_name.c_str());
      } else if (!new_value->IsType(user_pref_value->GetType())) {
        LOG(WARNING) << "Synced value for " << preference.name()
                     << " is of type " << new_value->GetType()
                     << " which doesn't match pref type "
                     << user_pref_value->GetType();
      } else if (!user_pref_value->Equals(new_value.get())) {
        pref_service_->Set(pref_name.c_str(), *new_value);
      }

      // If the merge resulted in an updated value, inform the syncer.
      if (!sync_value->Equals(new_value.get())) {
        syncer::SyncData sync_data;
        if (!CreatePrefSyncData(pref_name, *new_value, &sync_data)) {
          LOG(ERROR) << "Failed to update preference.";
          return;
        }

        std::string old_pref_name;
        if (client_ &&
            client_->IsMigratedPreference(pref_name, &old_pref_name)) {
          // This preference has been migrated from an old version that must be
          // kept in sync on older versions of Chrome.
          if (preference.name() == old_pref_name) {
            DCHECK(migrated_preference_list);
            // If the name the syncer has is the old pre-migration value, then
            // it's possible the new migrated preference name hasn't been synced
            // yet. In that case the SyncChange should be an ACTION_ADD rather
            // than an ACTION_UPDATE. Defer the decision of whether to sync with
            // ACTION_ADD or ACTION_UPDATE until the migrated_preferences phase.
            if (migrated_preference_list)
              (*migrated_preference_list)[pref_name] = sync_data;
          } else {
            DCHECK_EQ(preference.name(), pref_name);
            sync_changes->push_back(
                syncer::SyncChange(FROM_HERE,
                                   syncer::SyncChange::ACTION_UPDATE,
                                   sync_data));
          }

          syncer::SyncData old_sync_data;
          if (!CreatePrefSyncData(old_pref_name, *new_value, &old_sync_data)) {
            LOG(ERROR) << "Failed to update preference.";
            return;
          }
          if (migrated_preference_list)
            (*migrated_preference_list)[old_pref_name] = old_sync_data;
        } else {
          sync_changes->push_back(
            syncer::SyncChange(FROM_HERE,
                               syncer::SyncChange::ACTION_UPDATE,
                               sync_data));
        }
      }
    } else if (!sync_value->IsType(base::Value::TYPE_NULL)) {
      // Only a server value exists. Just set the local user value.
      pref_service_->Set(pref_name.c_str(), *sync_value);
    } else {
      LOG(WARNING) << "Sync has null value for pref " << pref_name.c_str();
    }
    synced_preferences_.insert(preference.name());
  } else if (user_pref_value) {
    // The server does not know about this preference and should be added
    // to the syncer's database.
    syncer::SyncData sync_data;
    if (!CreatePrefSyncData(pref_name, *user_pref_value, &sync_data)) {
      LOG(ERROR) << "Failed to update preference.";
      return;
    }
    sync_changes->push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_ADD,
                           sync_data));
    synced_preferences_.insert(pref_name);
  }

  // Else this pref does not have a sync value but also does not have a user
  // controlled value (either it's a default value or it's policy controlled,
  // either way it's not interesting). We can ignore it. Once it gets changed,
  // we'll send the new user controlled value to the syncer.
}

syncer::SyncMergeResult PrefModelAssociator::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK_EQ(type_, type);
  DCHECK(CalledOnValidThread());
  DCHECK(pref_service_);
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK(sync_error_factory.get());
  syncer::SyncMergeResult merge_result(type);
  sync_processor_ = sync_processor.Pass();
  sync_error_factory_ = sync_error_factory.Pass();

  syncer::SyncChangeList new_changes;
  std::set<std::string> remaining_preferences = registered_preferences_;

  // Maintains a list of old migrated preference names that we wish to sync.
  // Keep track of these in a list such that when the preference iteration
  // loops below are complete we can go back and determine whether
  SyncDataMap migrated_preference_list;

  // Go through and check for all preferences we care about that sync already
  // knows about.
  for (syncer::SyncDataList::const_iterator sync_iter =
           initial_sync_data.begin();
       sync_iter != initial_sync_data.end();
       ++sync_iter) {
    DCHECK_EQ(type_, sync_iter->GetDataType());

    const sync_pb::PreferenceSpecifics& preference = GetSpecifics(*sync_iter);
    std::string sync_pref_name = preference.name();

    if (remaining_preferences.count(sync_pref_name) == 0) {
      std::string new_pref_name;
      if (client_ &&
          client_->IsOldMigratedPreference(sync_pref_name, &new_pref_name)) {
        // This old pref name is not syncable locally anymore but we accept
        // changes from other Chrome installs of previous versions and migrate
        // them to the new name. Note that we will be merging any differences
        // between the new and old values and sync'ing them back.
        sync_pref_name = new_pref_name;
      } else {
        // We're not syncing this preference locally, ignore the sync data.
        // TODO(zea): Eventually we want to be able to have the syncable service
        // reconstruct all sync data for its datatype (therefore having
        // GetAllSyncData be a complete representation). We should store this
        // data somewhere, even if we don't use it.
        continue;
      }
    } else {
      remaining_preferences.erase(sync_pref_name);
    }
    InitPrefAndAssociate(*sync_iter, sync_pref_name, &new_changes,
                         &migrated_preference_list);
  }

  // Go through and build sync data for any remaining preferences.
  for (std::set<std::string>::iterator pref_name_iter =
          remaining_preferences.begin();
       pref_name_iter != remaining_preferences.end();
       ++pref_name_iter) {
    InitPrefAndAssociate(syncer::SyncData(), *pref_name_iter, &new_changes,
                         &migrated_preference_list);
  }

  // Now go over any migrated preference names and build sync data for them too.
  for (SyncDataMap::const_iterator migrated_pref_iter =
          migrated_preference_list.begin();
       migrated_pref_iter != migrated_preference_list.end();
       ++migrated_pref_iter) {
    syncer::SyncChange::SyncChangeType change_type =
        (synced_preferences_.count(migrated_pref_iter->first) == 0) ?
            syncer::SyncChange::ACTION_ADD :
            syncer::SyncChange::ACTION_UPDATE;
    new_changes.push_back(
        syncer::SyncChange(FROM_HERE, change_type, migrated_pref_iter->second));
    synced_preferences_.insert(migrated_pref_iter->first);
  }

  // Push updates to sync.
  merge_result.set_error(
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes));
  if (merge_result.error().IsSet())
    return merge_result;

  models_associated_ = true;
  pref_service_->OnIsSyncingChanged();
  return merge_result;
}

void PrefModelAssociator::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(type_, type);
  models_associated_ = false;
  sync_processor_.reset();
  sync_error_factory_.reset();
  pref_service_->OnIsSyncingChanged();
}

scoped_ptr<base::Value> PrefModelAssociator::MergePreference(
    const std::string& name,
    const base::Value& local_value,
    const base::Value& server_value) {
  // This function special cases preferences individually, so don't attempt
  // to merge for all migrated values.
  if (client_) {
    std::string new_pref_name;
    DCHECK(!client_->IsOldMigratedPreference(name, &new_pref_name));
    if (client_->IsMergeableListPreference(name))
      return make_scoped_ptr(MergeListValues(local_value, server_value));
    if (client_->IsMergeableDictionaryPreference(name))
      return make_scoped_ptr(MergeDictionaryValues(local_value, server_value));
  }

  // If this is not a specially handled preference, server wins.
  return make_scoped_ptr(server_value.DeepCopy());
}

bool PrefModelAssociator::CreatePrefSyncData(
    const std::string& name,
    const base::Value& value,
    syncer::SyncData* sync_data) const {
  if (value.IsType(base::Value::TYPE_NULL)) {
    LOG(ERROR) << "Attempting to sync a null pref value for " << name;
    return false;
  }

  std::string serialized;
  // TODO(zea): consider JSONWriter::Write since you don't have to check
  // failures to deserialize.
  JSONStringValueSerializer json(&serialized);
  if (!json.Serialize(value)) {
    LOG(ERROR) << "Failed to serialize preference value.";
    return false;
  }

  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref_specifics =
      GetMutableSpecifics(type_, &specifics);

  pref_specifics->set_name(name);
  pref_specifics->set_value(serialized);
  *sync_data = syncer::SyncData::CreateLocalData(name, name, specifics);
  return true;
}

base::Value* PrefModelAssociator::MergeListValues(const base::Value& from_value,
                                                  const base::Value& to_value) {
  if (from_value.GetType() == base::Value::TYPE_NULL)
    return to_value.DeepCopy();
  if (to_value.GetType() == base::Value::TYPE_NULL)
    return from_value.DeepCopy();

  DCHECK(from_value.GetType() == base::Value::TYPE_LIST);
  DCHECK(to_value.GetType() == base::Value::TYPE_LIST);
  const base::ListValue& from_list_value =
      static_cast<const base::ListValue&>(from_value);
  const base::ListValue& to_list_value =
      static_cast<const base::ListValue&>(to_value);
  base::ListValue* result = to_list_value.DeepCopy();

  for (base::ListValue::const_iterator i = from_list_value.begin();
       i != from_list_value.end(); ++i) {
    base::Value* value = (*i)->DeepCopy();
    result->AppendIfNotPresent(value);
  }
  return result;
}

base::Value* PrefModelAssociator::MergeDictionaryValues(
    const base::Value& from_value,
    const base::Value& to_value) {
  if (from_value.GetType() == base::Value::TYPE_NULL)
    return to_value.DeepCopy();
  if (to_value.GetType() == base::Value::TYPE_NULL)
    return from_value.DeepCopy();

  DCHECK_EQ(from_value.GetType(), base::Value::TYPE_DICTIONARY);
  DCHECK_EQ(to_value.GetType(), base::Value::TYPE_DICTIONARY);
  const base::DictionaryValue& from_dict_value =
      static_cast<const base::DictionaryValue&>(from_value);
  const base::DictionaryValue& to_dict_value =
      static_cast<const base::DictionaryValue&>(to_value);
  base::DictionaryValue* result = to_dict_value.DeepCopy();

  for (base::DictionaryValue::Iterator it(from_dict_value); !it.IsAtEnd();
       it.Advance()) {
    const base::Value* from_key_value = &it.value();
    base::Value* to_key_value;
    if (result->GetWithoutPathExpansion(it.key(), &to_key_value)) {
      if (from_key_value->GetType() == base::Value::TYPE_DICTIONARY &&
          to_key_value->GetType() == base::Value::TYPE_DICTIONARY) {
        base::Value* merged_value =
            MergeDictionaryValues(*from_key_value, *to_key_value);
        result->SetWithoutPathExpansion(it.key(), merged_value);
      }
      // Note that for all other types we want to preserve the "to"
      // values so we do nothing here.
    } else {
      result->SetWithoutPathExpansion(it.key(), from_key_value->DeepCopy());
    }
  }
  return result;
}

// Note: This will build a model of all preferences registered as syncable
// with user controlled data. We do not track any information for preferences
// not registered locally as syncable and do not inform the syncer of
// non-user controlled preferences.
syncer::SyncDataList PrefModelAssociator::GetAllSyncData(
    syncer::ModelType type)
    const {
  DCHECK_EQ(type_, type);
  syncer::SyncDataList current_data;
  for (PreferenceSet::const_iterator iter = synced_preferences_.begin();
       iter != synced_preferences_.end();
       ++iter) {
    std::string name = *iter;
    const PrefService::Preference* pref =
      pref_service_->FindPreference(name.c_str());
    DCHECK(pref);
    if (!pref->IsUserControlled() || pref->IsDefaultValue())
      continue;  // This is not data we care about.
    // TODO(zea): plumb a way to read the user controlled value.
    syncer::SyncData sync_data;
    if (!CreatePrefSyncData(name, *pref->GetValue(), &sync_data))
      continue;
    current_data.push_back(sync_data);
  }
  return current_data;
}

syncer::SyncError PrefModelAssociator::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!models_associated_) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Models not yet associated.",
                            PREFERENCES);
    return error;
  }
  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);
  syncer::SyncChangeList::const_iterator iter;
  for (iter = change_list.begin(); iter != change_list.end(); ++iter) {
    DCHECK_EQ(type_, iter->sync_data().GetDataType());

    const sync_pb::PreferenceSpecifics& pref_specifics =
        GetSpecifics(iter->sync_data());

    std::string name = pref_specifics.name();
    // It is possible that we may receive a change to a preference we do not
    // want to sync. For example if the user is syncing a Mac client and a
    // Windows client, the Windows client does not support
    // kConfirmToQuitEnabled. Ignore updates from these preferences.
    std::string pref_name = pref_specifics.name();
    std::string new_pref_name;
    // We migrated this preference name, so do as if the name had not changed.
    if (client_ && client_->IsOldMigratedPreference(name, &new_pref_name)) {
      pref_name = new_pref_name;
    }

    if (!IsPrefRegistered(pref_name.c_str()))
      continue;

    if (iter->change_type() == syncer::SyncChange::ACTION_DELETE) {
      pref_service_->ClearPref(pref_name);
      continue;
    }

    scoped_ptr<base::Value> value(ReadPreferenceSpecifics(pref_specifics));
    if (!value.get()) {
      // Skip values we can't deserialize.
      // TODO(zea): consider taking some further action such as erasing the bad
      // data.
      continue;
    }

    // This will only modify the user controlled value store, which takes
    // priority over the default value but is ignored if the preference is
    // policy controlled.
    pref_service_->Set(pref_name, *value);

    NotifySyncedPrefObservers(pref_specifics.name(), true /*from_sync*/);

    // Keep track of any newly synced preferences.
    if (iter->change_type() == syncer::SyncChange::ACTION_ADD) {
      synced_preferences_.insert(pref_specifics.name());
    }
  }
  return syncer::SyncError();
}

base::Value* PrefModelAssociator::ReadPreferenceSpecifics(
    const sync_pb::PreferenceSpecifics& preference) {
  base::JSONReader reader;
  scoped_ptr<base::Value> value(reader.ReadToValue(preference.value()));
  if (!value.get()) {
    std::string err = "Failed to deserialize preference value: " +
        reader.GetErrorMessage();
    LOG(ERROR) << err;
    return NULL;
  }
  return value.release();
}

bool PrefModelAssociator::IsPrefSynced(const std::string& name) const {
  return synced_preferences_.find(name) != synced_preferences_.end();
}

void PrefModelAssociator::AddSyncedPrefObserver(const std::string& name,
    SyncedPrefObserver* observer) {
  SyncedPrefObserverList* observers = synced_pref_observers_[name];
  if (observers == NULL) {
    observers = new SyncedPrefObserverList;
    synced_pref_observers_[name] = observers;
  }
  observers->AddObserver(observer);
}

void PrefModelAssociator::RemoveSyncedPrefObserver(const std::string& name,
    SyncedPrefObserver* observer) {
  SyncedPrefObserverMap::iterator observer_iter =
      synced_pref_observers_.find(name);
  if (observer_iter == synced_pref_observers_.end())
    return;
  SyncedPrefObserverList* observers = observer_iter->second;
  observers->RemoveObserver(observer);
}

void PrefModelAssociator::SetPrefModelAssociatorClientForTesting(
    const PrefModelAssociatorClient* client) {
  DCHECK(!client_);
  client_ = client;
}

std::set<std::string> PrefModelAssociator::registered_preferences() const {
  return registered_preferences_;
}

void PrefModelAssociator::RegisterPref(const char* name) {
  DCHECK(!models_associated_ && registered_preferences_.count(name) == 0);
  registered_preferences_.insert(name);
}

bool PrefModelAssociator::IsPrefRegistered(const char* name) {
  return registered_preferences_.count(name) > 0;
}

void PrefModelAssociator::ProcessPrefChange(const std::string& name) {
  if (processing_syncer_changes_)
    return;  // These are changes originating from us, ignore.

  // We only process changes if we've already associated models.
  if (!models_associated_)
    return;

  const PrefService::Preference* preference =
      pref_service_->FindPreference(name.c_str());
  if (!preference)
    return;

  if (!IsPrefRegistered(name.c_str()))
    return;  // We are not syncing this preference.

  syncer::SyncChangeList changes;

  if (!preference->IsUserModifiable()) {
    // If the preference is no longer user modifiable, it must now be controlled
    // by policy, whose values we do not sync. Just return. If the preference
    // stops being controlled by policy, it will revert back to the user value
    // (which we continue to update with sync changes).
    return;
  }

  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  NotifySyncedPrefObservers(name, false /*from_sync*/);

  if (synced_preferences_.count(name) == 0) {
    // Not in synced_preferences_ means no synced data. InitPrefAndAssociate(..)
    // will determine if we care about its data (e.g. if it has a default value
    // and hasn't been changed yet we don't) and take care syncing any new data.
    InitPrefAndAssociate(syncer::SyncData(), name, &changes, NULL);
  } else {
    // We are already syncing this preference, just update it's sync node.
    syncer::SyncData sync_data;
    if (!CreatePrefSyncData(name, *preference->GetValue(), &sync_data)) {
      LOG(ERROR) << "Failed to update preference.";
      return;
    }
    changes.push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_UPDATE,
                           sync_data));
    // This preference has been migrated from an old version that must be kept
    // in sync on older versions of Chrome.
    std::string old_pref_name;
    if (client_ && client_->IsMigratedPreference(name, &old_pref_name)) {
      if (!CreatePrefSyncData(old_pref_name,
                              *preference->GetValue(),
                              &sync_data)) {
        LOG(ERROR) << "Failed to update preference.";
        return;
      }

      syncer::SyncChange::SyncChangeType change_type =
          (synced_preferences_.count(old_pref_name) == 0) ?
              syncer::SyncChange::ACTION_ADD :
              syncer::SyncChange::ACTION_UPDATE;
      changes.push_back(
          syncer::SyncChange(FROM_HERE, change_type, sync_data));
    }
  }

  syncer::SyncError error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void PrefModelAssociator::SetPrefService(PrefServiceSyncable* pref_service) {
  DCHECK(pref_service_ == NULL);
  pref_service_ = pref_service;
}

void PrefModelAssociator::NotifySyncedPrefObservers(const std::string& path,
                                                    bool from_sync) const {
  SyncedPrefObserverMap::const_iterator observer_iter =
      synced_pref_observers_.find(path);
  if (observer_iter == synced_pref_observers_.end())
    return;
  SyncedPrefObserverList* observers = observer_iter->second;
  FOR_EACH_OBSERVER(SyncedPrefObserver, *observers,
                    OnSyncedPrefChanged(path, from_sync));
}

}  // namespace syncable_prefs
