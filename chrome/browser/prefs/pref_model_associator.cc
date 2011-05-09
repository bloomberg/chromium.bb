// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_model_associator.h"

#include "base/auto_reset.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/generic_change_processor.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/common/json_value_serializer.h"
#include "content/common/notification_service.h"

using syncable::PREFERENCES;

PrefModelAssociator::PrefModelAssociator()
    : pref_service_(NULL),
      sync_service_(NULL),
      models_associated_(false),
      processing_syncer_changes_(false),
      change_processor_(NULL) {
}

PrefModelAssociator::PrefModelAssociator(
    PrefService* pref_service)
    : pref_service_(pref_service),
      sync_service_(NULL),
      models_associated_(false),
      processing_syncer_changes_(false),
      change_processor_(NULL) {
  DCHECK(CalledOnValidThread());
}

PrefModelAssociator::~PrefModelAssociator() {
  DCHECK(CalledOnValidThread());
  change_processor_ = NULL;
  sync_service_ = NULL;
  pref_service_ = NULL;
}

bool PrefModelAssociator::InitPrefNodeAndAssociate(
    sync_api::WriteTransaction* trans,
    const sync_api::BaseNode& root,
    const PrefService::Preference* pref) {
  DCHECK(pref);

  base::JSONReader reader;
  std::string tag = pref->name();
  sync_api::WriteNode node(trans);
  if (node.InitByClientTagLookup(PREFERENCES, tag)) {
    // The server has a value for the preference.
    const sync_pb::PreferenceSpecifics& preference(
        node.GetPreferenceSpecifics());
    DCHECK_EQ(tag, preference.name());

    if (!pref->IsUserModifiable()) {
      Associate(pref, node.GetId());
      return true;
    }

    scoped_ptr<Value> value(
        reader.JsonToValue(preference.value(), false, false));
    std::string pref_name = preference.name();
    if (!value.get()) {
      LOG(ERROR) << "Failed to deserialize preference value: "
                 << reader.GetErrorMessage();
      return false;
    }

    // Merge the server value of this preference with the local value.
    scoped_ptr<Value> new_value(MergePreference(*pref, *value));

    // Update the local preference based on what we got from the
    // sync server.
    if (new_value->IsType(Value::TYPE_NULL)) {
      pref_service_->ClearPref(pref_name.c_str());
    } else if (!new_value->IsType(pref->GetType())) {
      LOG(WARNING) << "Synced value for " << preference.name()
                   << " is of type " << new_value->GetType()
                   << " which doesn't match pref type " << pref->GetType();
    } else if (!pref->GetValue()->Equals(new_value.get())) {
      pref_service_->Set(pref_name.c_str(), *new_value);
    }

    SendUpdateNotificationsIfNecessary(pref_name);

    // If the merge resulted in an updated value, write it back to
    // the sync node.
    if (!value->Equals(new_value.get()) &&
        !WritePreferenceToNode(pref->name(), *new_value, &node)) {
      return false;
    }
    Associate(pref, node.GetId());
  } else if (pref->IsUserControlled()) {
    // The server doesn't have a value, but we have a user-controlled value,
    // so we push it to the server.
    sync_api::WriteNode write_node(trans);
    if (!write_node.InitUniqueByCreation(PREFERENCES, root, tag)) {
      LOG(ERROR) << "Failed to create preference sync node.";
      return false;
    }

    // Update the sync node with the local value for this preference.
    if (!WritePreferenceToNode(pref->name(), *pref->GetValue(), &write_node))
      return false;

    Associate(pref, write_node.GetId());
  } else {
    // This preference is handled by policy, not the user, and therefore
    // we do not associate it.
  }

  return true;
}

bool PrefModelAssociator::AssociateModels() {
  DCHECK(CalledOnValidThread());

  int64 root_id;
  if (!GetSyncIdForTaggedNode(syncable::ModelTypeToRootTag(PREFERENCES),
                              &root_id)) {
    LOG(ERROR) << "Server did not create the top-level preferences node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  sync_api::WriteTransaction trans(sync_service_->GetUserShare());
  sync_api::ReadNode root(&trans);
  if (!root.InitByIdLookup(root_id)) {
    LOG(ERROR) << "Server did not create the top-level preferences node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  for (std::set<std::string>::iterator it = synced_preferences_.begin();
       it != synced_preferences_.end(); ++it) {
    std::string name = *it;
    const PrefService::Preference* pref =
        pref_service_->FindPreference(name.c_str());
    VLOG(1) << "Associating preference " << name;
    DCHECK(pref);
    if (!pref->IsUserModifiable())
      continue;  // We don't sync preferences the user cannot change.
    InitPrefNodeAndAssociate(&trans, root, pref);
  }
  models_associated_ = true;
  return true;
}

bool PrefModelAssociator::DisassociateModels() {
  id_map_.clear();
  id_map_inverse_.clear();
  models_associated_ = false;
  return true;
}

bool PrefModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(has_nodes);
  *has_nodes = false;
  int64 preferences_sync_id;
  if (!GetSyncIdForTaggedNode(syncable::ModelTypeToRootTag(PREFERENCES),
                              &preferences_sync_id)) {
    LOG(ERROR) << "Server did not create the top-level preferences node. We "
               << "might be running against an out-of-date server.";
    return false;
  }
  sync_api::ReadTransaction trans(sync_service_->GetUserShare());

  sync_api::ReadNode preferences_node(&trans);
  if (!preferences_node.InitByIdLookup(preferences_sync_id)) {
    LOG(ERROR) << "Server did not create the top-level preferences node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  // The sync model has user created nodes if the preferences folder has any
  // children.
  *has_nodes = sync_api::kInvalidId != preferences_node.GetFirstChildId();
  return true;
}

int64 PrefModelAssociator::GetSyncIdFromChromeId(
    const std::string& preference_name) {
  PreferenceNameToSyncIdMap::const_iterator iter =
      id_map_.find(preference_name);
  return iter == id_map_.end() ? sync_api::kInvalidId : iter->second;
}

void PrefModelAssociator::Associate(
    const PrefService::Preference* preference, int64 sync_id) {
  DCHECK(CalledOnValidThread());

  std::string name = preference->name();
  DCHECK_NE(sync_api::kInvalidId, sync_id);
  DCHECK_EQ(0U, id_map_.count(name));
  DCHECK_EQ(0U, id_map_inverse_.count(sync_id));
  id_map_[name] = sync_id;
  id_map_inverse_[sync_id] = name;
}

void PrefModelAssociator::Disassociate(int64 sync_id) {
  DCHECK(CalledOnValidThread());
  SyncIdToPreferenceNameMap::iterator iter = id_map_inverse_.find(sync_id);
  if (iter == id_map_inverse_.end())
    return;
  id_map_.erase(iter->second);
  id_map_inverse_.erase(iter);
}

bool PrefModelAssociator::GetSyncIdForTaggedNode(const std::string& tag,
                                                 int64* sync_id) {
  sync_api::ReadTransaction trans(sync_service_->GetUserShare());
  sync_api::ReadNode sync_node(&trans);
  if (!sync_node.InitByTagLookup(tag.c_str()))
    return false;
  *sync_id = sync_node.GetId();
  return true;
}

Value* PrefModelAssociator::MergePreference(
    const PrefService::Preference& local_pref,
    const Value& server_value) {
  const std::string& name(local_pref.name());
  if (name == prefs::kURLsToRestoreOnStartup ||
      name == prefs::kDesktopNotificationAllowedOrigins ||
      name == prefs::kDesktopNotificationDeniedOrigins) {
    return MergeListValues(*local_pref.GetValue(), server_value);
  }

  if (name == prefs::kContentSettingsPatterns ||
      name == prefs::kGeolocationContentSettings) {
    return MergeDictionaryValues(*local_pref.GetValue(), server_value);
  }

  // If this is not a specially handled preference, server wins.
  return server_value.DeepCopy();
}

bool PrefModelAssociator::WritePreferenceToNode(
    const std::string& name,
    const Value& value,
    sync_api::WriteNode* node) {
  std::string serialized;
  JSONStringValueSerializer json(&serialized);
  if (!json.Serialize(value)) {
    LOG(ERROR) << "Failed to serialize preference value.";
    return false;
  }

  sync_pb::PreferenceSpecifics preference;
  preference.set_name(name);
  preference.set_value(serialized);
  node->SetPreferenceSpecifics(preference);
  // TODO(viettrungluu): eliminate conversion (it's temporary)
  node->SetTitle(UTF8ToWide(name));
  return true;
}

Value* PrefModelAssociator::MergeListValues(const Value& from_value,
                                            const Value& to_value) {
  if (from_value.GetType() == Value::TYPE_NULL)
    return to_value.DeepCopy();
  if (to_value.GetType() == Value::TYPE_NULL)
    return from_value.DeepCopy();

  DCHECK(from_value.GetType() == Value::TYPE_LIST);
  DCHECK(to_value.GetType() == Value::TYPE_LIST);
  const ListValue& from_list_value = static_cast<const ListValue&>(from_value);
  const ListValue& to_list_value = static_cast<const ListValue&>(to_value);
  ListValue* result = to_list_value.DeepCopy();

  for (ListValue::const_iterator i = from_list_value.begin();
       i != from_list_value.end(); ++i) {
    Value* value = (*i)->DeepCopy();
    result->AppendIfNotPresent(value);
  }
  return result;
}

Value* PrefModelAssociator::MergeDictionaryValues(
    const Value& from_value,
    const Value& to_value) {
  if (from_value.GetType() == Value::TYPE_NULL)
    return to_value.DeepCopy();
  if (to_value.GetType() == Value::TYPE_NULL)
    return from_value.DeepCopy();

  DCHECK_EQ(from_value.GetType(), Value::TYPE_DICTIONARY);
  DCHECK_EQ(to_value.GetType(), Value::TYPE_DICTIONARY);
  const DictionaryValue& from_dict_value =
      static_cast<const DictionaryValue&>(from_value);
  const DictionaryValue& to_dict_value =
      static_cast<const DictionaryValue&>(to_value);
  DictionaryValue* result = to_dict_value.DeepCopy();

  for (DictionaryValue::key_iterator key = from_dict_value.begin_keys();
       key != from_dict_value.end_keys(); ++key) {
    Value* from_value;
    bool success = from_dict_value.GetWithoutPathExpansion(*key, &from_value);
    DCHECK(success);

    Value* to_key_value;
    if (result->GetWithoutPathExpansion(*key, &to_key_value)) {
      if (to_key_value->GetType() == Value::TYPE_DICTIONARY) {
        Value* merged_value = MergeDictionaryValues(*from_value, *to_key_value);
        result->SetWithoutPathExpansion(*key, merged_value);
      }
      // Note that for all other types we want to preserve the "to"
      // values so we do nothing here.
    } else {
      result->SetWithoutPathExpansion(*key, from_value->DeepCopy());
    }
  }
  return result;
}

void PrefModelAssociator::SendUpdateNotificationsIfNecessary(
    const std::string& pref_name) {
  // The bookmark bar visibility preference requires a special
  // notification to update the UI.
  if (0 == pref_name.compare(prefs::kShowBookmarkBar)) {
    NotificationService::current()->Notify(
        NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
        Source<PrefModelAssociator>(this),
        NotificationService::NoDetails());
  }
}

// Not implemented.
void PrefModelAssociator::AbortAssociation() {}

bool PrefModelAssociator::CryptoReadyIfNecessary() {
  // We only access the cryptographer while holding a transaction.
  sync_api::ReadTransaction trans(sync_service_->GetUserShare());
  syncable::ModelTypeSet encrypted_types;
  sync_service_->GetEncryptedDataTypes(&encrypted_types);
  return encrypted_types.count(PREFERENCES) == 0 ||
         sync_service_->IsCryptographerReady(&trans);
}

void PrefModelAssociator::SetupSync(
    ProfileSyncService* sync_service,
    browser_sync::GenericChangeProcessor* change_processor) {
  sync_service_ = sync_service;
  change_processor_ = change_processor;
}

void PrefModelAssociator::ApplyChangesFromSync(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  if (!models_associated_)
    return;
  AutoReset<bool> processing_changes(&processing_syncer_changes_, true);
  for (int i = 0; i < change_count; ++i) {
    if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE ==
        changes[i].action) {
      // We never delete preferences.
      NOTREACHED();
    }

    sync_api::ReadNode node(trans);
    if (!node.InitByIdLookup(changes[i].id)) {
      LOG(ERROR) << "Preference node lookup failed.";
      return;
    }
    DCHECK(PREFERENCES == node.GetModelType());
    std::string name;
    sync_pb::PreferenceSpecifics pref_specifics =
        node.GetPreferenceSpecifics();
    scoped_ptr<Value> value(ReadPreferenceSpecifics(pref_specifics,
                                                    &name));
    // Skip values we can't deserialize.
    if (!value.get())
      continue;

    // It is possible that we may receive a change to a preference we do not
    // want to sync. For example if the user is syncing a Mac client and a
    // Windows client, the Windows client does not support
    // kConfirmToQuitEnabled. Ignore updates from these preferences.
    const char* pref_name = name.c_str();
    if (!IsPrefRegistered(pref_name))
      continue;

    const PrefService::Preference* pref =
        pref_service_->FindPreference(pref_name);
    DCHECK(pref);
    if (!pref->IsUserModifiable()) {
      continue;
    }

    if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE ==
        changes[i].action) {
      pref_service_->ClearPref(pref_name);
    } else {
      pref_service_->Set(pref_name, *value);

      // If this is a newly added node, associate.
      if (sync_api::SyncManager::ChangeRecord::ACTION_ADD ==
          changes[i].action) {
        Associate(pref, changes[i].id);
      }

      SendUpdateNotificationsIfNecessary(name);
    }
  }
}

Value* PrefModelAssociator::ReadPreferenceSpecifics(
    const sync_pb::PreferenceSpecifics& preference,
    std::string* name) {
  base::JSONReader reader;
  scoped_ptr<Value> value(reader.JsonToValue(preference.value(), false, false));
  if (!value.get()) {
    std::string err = "Failed to deserialize preference value: " +
        reader.GetErrorMessage();
    LOG(ERROR) << err;
    return NULL;
  }
  *name = preference.name();
  return value.release();
}


std::set<std::string> PrefModelAssociator::synced_preferences() const {
  return synced_preferences_;
}

void PrefModelAssociator::RegisterPref(const char* name) {
  DCHECK(!models_associated_ && synced_preferences_.count(name) == 0);
  synced_preferences_.insert(name);
}

bool PrefModelAssociator::IsPrefRegistered(const char* name) {
  return synced_preferences_.count(name) > 0;
}

void PrefModelAssociator::ProcessPrefChange(const std::string& name) {
  if (processing_syncer_changes_)
    return;  // These are changes originating from us, ignore.

  // We only process changes if we've already associated models.
  if (!sync_service_ || !models_associated_)
    return;

  const PrefService::Preference* preference =
      pref_service_->FindPreference(name.c_str());
  if (!IsPrefRegistered(name.c_str()))
    return;  // We are not syncing this preference.

  // The preference does not have a node. This can happen if the preference
  // held the default value at association time. Create one and associate.
  int64 root_id;
  if (!GetSyncIdForTaggedNode(syncable::ModelTypeToRootTag(PREFERENCES),
                              &root_id)) {
    LOG(ERROR) << "Server did not create the top-level preferences node. We "
               << "might be running against an out-of-date server.";
    return;
  }

  int64 sync_id = GetSyncIdFromChromeId(name);
  if (!preference->IsUserModifiable()) {
    // If the preference is not currently user modifiable, disassociate, so that
    // if it becomes user modifiable me pick up the server value.
    Disassociate(sync_id);
    return;
  }

  AutoReset<bool> processing_changes(&processing_syncer_changes_, true);
  sync_api::WriteTransaction trans(sync_service_->GetUserShare());

  // Since we don't create sync nodes for preferences that are not under control
  // of the user or still have their default value, this changed preference may
  // not have a sync node yet. If so, we create a node. Similarly, a preference
  // may become user-modifiable (e.g. due to laxer policy configuration), in
  // which case we also need to create a sync node and associate it.
  if (sync_id == sync_api::kInvalidId) {
    sync_api::ReadNode root(&trans);
    if (!root.InitByIdLookup(root_id)) {
      LOG(ERROR) << "Server did not create the top-level preferences node. We "
                 << "might be running against an out-of-date server.";
      return;
    }
    InitPrefNodeAndAssociate(&trans, root, preference);
  } else {
    sync_api::WriteNode node(&trans);
    if (!node.InitByIdLookup(sync_id)) {
      LOG(ERROR) << "Preference node lookup failed.";
      return;
    }

    if (!WritePreferenceToNode(name, *preference->GetValue(), &node)) {
      LOG(ERROR) << "Failed to update preference node.";
      return;
    }
  }
}
