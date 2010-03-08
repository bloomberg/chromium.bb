// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/preference_model_associator.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/pref_names.h"

namespace browser_sync {

PreferenceModelAssociator::PreferenceModelAssociator(
    ProfileSyncService* sync_service,
    UnrecoverableErrorHandler* error_handler)
    : sync_service_(sync_service),
      error_handler_(error_handler),
      preferences_node_id_(sync_api::kInvalidId),
      ALLOW_THIS_IN_INITIALIZER_LIST(persist_associations_(this)) {
  DCHECK(sync_service_);
  synced_preferences_.insert(prefs::kHomePageIsNewTabPage);
  synced_preferences_.insert(prefs::kHomePage);
  synced_preferences_.insert(prefs::kRestoreOnStartup);
  synced_preferences_.insert(prefs::kURLsToRestoreOnStartup);
  synced_preferences_.insert(prefs::kShowBookmarkBar);
  synced_preferences_.insert(prefs::kShowHomeButton);
}

bool PreferenceModelAssociator::AssociateModels() {
  // TODO(albertb): Attempt to load the model association from storage.
  PrefService* pref_service = sync_service_->profile()->GetPrefs();

  int64 root_id;
  if (!GetSyncIdForTaggedNode(kPreferencesTag, &root_id)) {
    error_handler_->OnUnrecoverableError();
    LOG(ERROR) << "Server did not create the top-level preferences node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  sync_api::WriteTransaction trans(
      sync_service()->backend()->GetUserShareHandle());
  sync_api::ReadNode root(&trans);
  if (!root.InitByIdLookup(root_id)) {
    error_handler_->OnUnrecoverableError();
    LOG(ERROR) << "Server did not create the top-level preferences node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  base::JSONReader reader;
  for (std::set<std::wstring>::iterator it = synced_preferences_.begin();
       it != synced_preferences_.end(); ++it) {
    std::string tag = WideToUTF8(*it);

    sync_api::ReadNode node(&trans);
    if (node.InitByClientTagLookup(syncable::PREFERENCES, tag)) {
      const sync_pb::PreferenceSpecifics& preference(
          node.GetPreferenceSpecifics());
      DCHECK_EQ(tag, preference.name());

      scoped_ptr<Value> value(
          reader.JsonToValue(preference.value(), false, false));
      std::wstring pref_name = UTF8ToWide(preference.name());
      if (!value.get()) {
        LOG(ERROR) << "Failed to deserialize preference value: "
                   << reader.error_message();
        error_handler_->OnUnrecoverableError();
        return false;
      }

      // Update the local preference based on what we got from the sync server.
      const PrefService::Preference* pref =
          pref_service->FindPreference((*it).c_str());
      if (!pref) {
        LOG(ERROR) << "Unrecognized preference -- ignoring.";
        continue;
      }

      pref_service->Set(pref_name.c_str(), *value);
      Associate(pref, node.GetId());
    } else {
      sync_api::WriteNode node(&trans);
      if (!node.InitUniqueByCreation(syncable::PREFERENCES, root, tag)) {
        LOG(ERROR) << "Failed to create preference sync node.";
        error_handler_->OnUnrecoverableError();
        return false;
      }

      // Update the sync node with the local value for this preference.
      const PrefService::Preference* pref =
          pref_service->FindPreference((*it).c_str());
      if (!pref) {
        LOG(ERROR) << "Unrecognized preference -- ignoring.";
        continue;
      }

      std::string serialized;
      JSONStringValueSerializer json(&serialized);
      if (!json.Serialize(*(pref->GetValue()))) {
        LOG(ERROR) << "Failed to serialize preference value.";
        error_handler_->OnUnrecoverableError();
        return false;
      }
      sync_pb::PreferenceSpecifics preference;
      preference.set_name(tag);
      preference.set_value(serialized);
      node.SetPreferenceSpecifics(preference);
      node.SetTitle(*it);
      Associate(pref, node.GetId());
    }
  }
  return true;
}

bool PreferenceModelAssociator::DisassociateModels() {
  id_map_.clear();
  id_map_inverse_.clear();
  dirty_associations_sync_ids_.clear();
  return true;
}

bool PreferenceModelAssociator::SyncModelHasUserCreatedNodes() {
  int64 preferences_sync_id;
  if (!GetSyncIdForTaggedNode(kPreferencesTag, &preferences_sync_id)) {
    sync_service()->OnUnrecoverableError();
    LOG(ERROR) << "Server did not create the top-level preferences node. We "
               << "might be running against an out-of-date server.";
    return false;
  }
  sync_api::ReadTransaction trans(
      sync_service()->backend()->GetUserShareHandle());

  sync_api::ReadNode preferences_node(&trans);
  if (!preferences_node.InitByIdLookup(preferences_sync_id)) {
    sync_service()->OnUnrecoverableError();
    LOG(ERROR) << "Server did not create the top-level preferences node. We "
               << "might be running against an out-of-date server.";
  }

  // The sync model has user created nodes if the preferences folder has any
  // children.
  return sync_api::kInvalidId != preferences_node.GetFirstChildId();
}

bool PreferenceModelAssociator::ChromeModelHasUserCreatedNodes() {
  // Assume the preferences model always have user-created nodes.
  return true;
}

int64 PreferenceModelAssociator::GetSyncIdFromChromeId(
    const std::wstring preference_name) {
  PreferenceNameToSyncIdMap::const_iterator iter =
      id_map_.find(preference_name);
  return iter == id_map_.end() ? sync_api::kInvalidId : iter->second;
}

void PreferenceModelAssociator::Associate(
    const PrefService::Preference* preference, int64 sync_id) {
  DCHECK_NE(sync_api::kInvalidId, sync_id);
  DCHECK(id_map_.find(preference->name()) == id_map_.end());
  DCHECK(id_map_inverse_.find(sync_id) == id_map_inverse_.end());
  id_map_[preference->name()] = sync_id;
  id_map_inverse_[sync_id] = preference->name();
  dirty_associations_sync_ids_.insert(sync_id);
  PostPersistAssociationsTask();
}

void PreferenceModelAssociator::Disassociate(int64 sync_id) {
  SyncIdToPreferenceNameMap::iterator iter = id_map_inverse_.find(sync_id);
  if (iter == id_map_inverse_.end())
    return;
  id_map_.erase(iter->second);
  id_map_inverse_.erase(iter);
  dirty_associations_sync_ids_.erase(sync_id);
}

bool PreferenceModelAssociator::GetSyncIdForTaggedNode(const std::string& tag,
                                                       int64* sync_id) {
  sync_api::ReadTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  sync_api::ReadNode sync_node(&trans);
  if (!sync_node.InitByTagLookup(tag.c_str()))
    return false;
  *sync_id = sync_node.GetId();
  return true;
}

void PreferenceModelAssociator::PostPersistAssociationsTask() {
  // No need to post a task if a task is already pending.
  if (!persist_associations_.empty())
    return;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      persist_associations_.NewRunnableMethod(
          &PreferenceModelAssociator::PersistAssociations));
}

void PreferenceModelAssociator::PersistAssociations() {
  // If there are no dirty associations we have nothing to do. We handle this
  // explicity instead of letting the for loop do it to avoid creating a write
  // transaction in this case.
  if (dirty_associations_sync_ids_.empty()) {
    DCHECK(id_map_.empty());
    DCHECK(id_map_inverse_.empty());
    return;
  }

  sync_api::WriteTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  DirtyAssociationsSyncIds::iterator iter;
  for (iter = dirty_associations_sync_ids_.begin();
       iter != dirty_associations_sync_ids_.end();
       ++iter) {
    int64 sync_id = *iter;
    sync_api::WriteNode sync_node(&trans);
    if (!sync_node.InitByIdLookup(sync_id)) {
      error_handler_->OnUnrecoverableError();
      return;
    }
    // TODO(sync): Make ExternalId a string?
    // sync_node.SetExternalId(id_map_inverse_[*iter]);
  }
  dirty_associations_sync_ids_.clear();
}

}  // namespace browser_sync
