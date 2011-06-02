// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/generic_change_processor.h"

#include "chrome/browser/sync/api/syncable_service.h"
#include "chrome/browser/sync/api/sync_change.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/syncable/nigori_util.h"

namespace browser_sync {

GenericChangeProcessor::GenericChangeProcessor(
    SyncableService* local_service,
    UnrecoverableErrorHandler* error_handler,
    sync_api::UserShare* user_share)
    : ChangeProcessor(error_handler),
      local_service_(local_service),
      user_share_(user_share) {
  DCHECK(local_service_);
}

GenericChangeProcessor::~GenericChangeProcessor() {
  // Set to null to ensure it's not used after destruction.
  local_service_ = NULL;
}

void GenericChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  DCHECK(running());
  DCHECK(syncer_changes_.empty());
  for (int i = 0; i < change_count; ++i) {
    SyncChange::SyncChangeType action;
    sync_pb::EntitySpecifics const* specifics = NULL;
    if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE ==
        changes[i].action) {
      action = SyncChange::ACTION_DELETE;
      specifics = &changes[i].specifics;
      DCHECK(specifics);
    } else if (sync_api::SyncManager::ChangeRecord::ACTION_ADD ==
               changes[i].action) {
      action = SyncChange::ACTION_ADD;
    } else {  // ACTION_UPDATE.
      action = SyncChange::ACTION_UPDATE;
    }
    if (!specifics) {
      // Need to load from node.
      sync_api::ReadNode read_node(trans);
      if (!read_node.InitByIdLookup(changes[i].id)) {
        error_handler()->OnUnrecoverableError(FROM_HERE, "Failed to look up "
            " data for received change with id " + changes[i].id);
        return;
      }
      specifics = &read_node.GetEntitySpecifics();
    }
    DCHECK_NE(syncable::UNSPECIFIED,
              syncable::GetModelTypeFromSpecifics(*specifics));
    syncer_changes_.push_back(
        SyncChange(action, SyncData::CreateRemoteData(*specifics)));
  }
}

void GenericChangeProcessor::CommitChangesFromSyncModel() {
  if (!running())
    return;
  if (syncer_changes_.empty())
    return;
  local_service_->ProcessSyncChanges(syncer_changes_);
  syncer_changes_.clear();
}

bool GenericChangeProcessor::GetSyncDataForType(
    syncable::ModelType type,
    SyncDataList* current_sync_data) {
  std::string type_name = syncable::ModelTypeToString(type);
  sync_api::ReadTransaction trans(share_handle());
  sync_api::ReadNode root(&trans);
  if (!root.InitByTagLookup(syncable::ModelTypeToRootTag(type))) {
    LOG(ERROR) << "Server did not create the top-level " + type_name + " node."
               << " We might be running against an out-of-date server.";
    return false;
  }

  int64 sync_child_id = root.GetFirstChildId();
  while (sync_child_id != sync_api::kInvalidId) {
    sync_api::ReadNode sync_child_node(&trans);
    if (!sync_child_node.InitByIdLookup(sync_child_id)) {
      LOG(ERROR) << "Failed to fetch child node for type " + type_name + ".";
      return false;
    }
    current_sync_data->push_back(SyncData::CreateRemoteData(
        sync_child_node.GetEntitySpecifics()));
    sync_child_id = sync_child_node.GetSuccessorId();
  }
  return true;
}

void GenericChangeProcessor::ProcessSyncChanges(
    const SyncChangeList& list_of_changes) {
  sync_api::WriteTransaction trans(share_handle());

  for (SyncChangeList::const_iterator iter = list_of_changes.begin();
       iter != list_of_changes.end();
       ++iter) {
    const SyncChange& change = *iter;
    DCHECK_NE(change.sync_data().GetDataType(), syncable::UNSPECIFIED);
    std::string type_str = syncable::ModelTypeToString(
        change.sync_data().GetDataType());
    sync_api::WriteNode sync_node(&trans);
    if (change.change_type() == SyncChange::ACTION_DELETE) {
      if (change.sync_data().GetTag() == "" ||
          !sync_node.InitByClientTagLookup(change.sync_data().GetDataType(),
                                           change.sync_data().GetTag())) {
        NOTREACHED();
        error_handler()->OnUnrecoverableError(FROM_HERE,
            "Failed to delete " + type_str + " node.");
        return;
      }
      sync_node.Remove();
    } else if (change.change_type() == SyncChange::ACTION_ADD) {
      // TODO(sync): Handle other types of creation (custom parents, folders,
      // etc.).
      sync_api::ReadNode root_node(&trans);
      if (!root_node.InitByTagLookup(
              syncable::ModelTypeToRootTag(change.sync_data().GetDataType()))) {
        NOTREACHED();
        error_handler()->OnUnrecoverableError(FROM_HERE,
            "Failed to look up root node for type " + type_str);
        return;
      }
      if (!sync_node.InitUniqueByCreation(change.sync_data().GetDataType(),
                                          root_node,
                                          change.sync_data().GetTag())) {
        error_handler()->OnUnrecoverableError(FROM_HERE,
            "Failed to create " + type_str + " node.");
        return;
      }
      sync_node.SetEntitySpecifics(change.sync_data().GetSpecifics());
    } else if (change.change_type() == SyncChange::ACTION_UPDATE) {
      if (change.sync_data().GetTag() == "" ||
          !sync_node.InitByClientTagLookup(change.sync_data().GetDataType(),
                                           change.sync_data().GetTag())) {
        NOTREACHED();
        error_handler()->OnUnrecoverableError(FROM_HERE,
            "Failed to update " + type_str + " node");
        return;
      }
      sync_node.SetEntitySpecifics(change.sync_data().GetSpecifics());
      // TODO(sync): Support updating other parts of the sync node (title,
      // successor, parent, etc.).
    } else {
      NOTREACHED();
      error_handler()->OnUnrecoverableError(FROM_HERE,
          "Received unset SyncChange in the change processor.");
      return;
    }
  }
}

bool GenericChangeProcessor::SyncModelHasUserCreatedNodes(
    syncable::ModelType type,
    bool* has_nodes) {
  DCHECK(has_nodes);
  DCHECK_NE(type, syncable::UNSPECIFIED);
  std::string type_name = syncable::ModelTypeToString(type);
  std::string err_str = "Server did not create the top-level " + type_name +
      " node. We might be running against an out-of-date server.";
  *has_nodes = false;
  sync_api::ReadTransaction trans(share_handle());
  sync_api::ReadNode type_root_node(&trans);
  if (!type_root_node.InitByTagLookup(syncable::ModelTypeToRootTag(type))) {
    LOG(ERROR) << err_str;
    return false;
  }

  // The sync model has user created nodes if the type's root node has any
  // children.
  *has_nodes = sync_api::kInvalidId != type_root_node.GetFirstChildId();
  return true;
}

bool GenericChangeProcessor::CryptoReadyIfNecessary(syncable::ModelType type) {
  DCHECK_NE(type, syncable::UNSPECIFIED);
  // We only access the cryptographer while holding a transaction.
  sync_api::ReadTransaction trans(share_handle());
  const syncable::ModelTypeSet& encrypted_types =
      syncable::GetEncryptedDataTypes(trans.GetWrappedTrans());
  return encrypted_types.count(type) == 0 ||
         trans.GetCryptographer()->is_ready();
}

void GenericChangeProcessor::StartImpl(Profile* profile) {}

void GenericChangeProcessor::StopImpl() {}

sync_api::UserShare* GenericChangeProcessor::share_handle() {
  return user_share_;
}

}  // namespace browser_sync
