// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/preference_model_associator.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

namespace browser_sync {

static const char kPreferenceName[] = "preference_name";
static const char kPreferenceValue[] = "preference_value";

PreferenceModelAssociator::PreferenceModelAssociator(
    ProfileSyncService* sync_service)
    : sync_service_(sync_service),
      preferences_node_id_(sync_api::kInvalidId),
      ALLOW_THIS_IN_INITIALIZER_LIST(persist_associations_(this)) {
  DCHECK(sync_service_);
  synced_preferences_.insert(prefs::kHomePageIsNewTabPage);
  synced_preferences_.insert(prefs::kHomePage);
  synced_preferences_.insert(prefs::kRestoreOnStartup);
  synced_preferences_.insert(prefs::kURLsToRestoreOnStartup);
  synced_preferences_.insert(prefs::kShowBookmarkBar);
}

bool PreferenceModelAssociator::AssociateModels() {

  // TODO(albertb): Attempt to load the model association from storage.
  sync_api::ReadTransaction trans(
      sync_service()->backend()->GetUserShareHandle());
  sync_api::ReadNode root_node(&trans);
  if (!root_node.InitByTagLookup(kPreferencesTag)) {
    sync_service_->OnUnrecoverableError();
    LOG(ERROR) << "Server did not create the top-level preferences node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  int64 pref_id = root_node.GetFirstChildId();
  while (sync_api::kInvalidId != pref_id) {
    sync_api::ReadNode pref_node(&trans);
    if (!pref_node.InitByIdLookup(pref_id)) {
      sync_service_->OnUnrecoverableError();
      LOG(ERROR) << "Preference node lookup failed.";
      return false;
    }
    // TODO(albertb): Load the preference name and value from the node.
    // TODO(albertb): Associate the preference name with pref_id.
    // TODO(albertb): Add the preference to synced_preferences if needed.
    pref_id = pref_node.GetSuccessorId();
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

int64 PreferenceModelAssociator::GetSyncIdForChromeId(
    const std::wstring preference_name) {
  PreferenceNameToSyncIdMap::const_iterator iter =
      id_map_.find(preference_name);
  return iter == id_map_.end() ? sync_api::kInvalidId : iter->second;
}

void PreferenceModelAssociator::Associate(std::wstring preference_name,
                                          int64 sync_id) {
  DCHECK_NE(sync_api::kInvalidId, sync_id);
  DCHECK(id_map_.find(preference_name) == id_map_.end());
  DCHECK(id_map_inverse_.find(sync_id) == id_map_inverse_.end());
  id_map_[preference_name] = sync_id;
  id_map_inverse_[sync_id] = preference_name;
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
      sync_service_->OnUnrecoverableError();
      return;
    }
    // TODO(sync): Make ExternalId a string?
    // sync_node.SetExternalId(id_map_inverse_[*iter]);
  }
  dirty_associations_sync_ids_.clear();
}

}  // namespace browser_sync
