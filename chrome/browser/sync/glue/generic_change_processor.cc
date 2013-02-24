// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/generic_change_processor.h"

#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"
#include "sync/internal_api/public/base_node.h"
#include "sync/internal_api/public/change_record.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/syncable/entry.h"  // TODO(tim): Bug 123674.

using content::BrowserThread;

namespace browser_sync {

GenericChangeProcessor::GenericChangeProcessor(
    DataTypeErrorHandler* error_handler,
    const base::WeakPtr<syncer::SyncableService>& local_service,
    const base::WeakPtr<syncer::SyncMergeResult>& merge_result,
    syncer::UserShare* user_share)
    : ChangeProcessor(error_handler),
      local_service_(local_service),
      merge_result_(merge_result),
      share_handle_(user_share) {
  DCHECK(CalledOnValidThread());
}

GenericChangeProcessor::~GenericChangeProcessor() {
  DCHECK(CalledOnValidThread());
}

void GenericChangeProcessor::ApplyChangesFromSyncModel(
    const syncer::BaseTransaction* trans,
    int64 model_version,
    const syncer::ImmutableChangeRecordList& changes) {
  DCHECK(CalledOnValidThread());
  DCHECK(syncer_changes_.empty());
  for (syncer::ChangeRecordList::const_iterator it =
           changes.Get().begin(); it != changes.Get().end(); ++it) {
    if (it->action == syncer::ChangeRecord::ACTION_DELETE) {
      syncer_changes_.push_back(
          syncer::SyncChange(
              FROM_HERE,
              syncer::SyncChange::ACTION_DELETE,
              syncer::SyncData::CreateRemoteData(it->id, it->specifics)));
    } else {
      syncer::SyncChange::SyncChangeType action =
          (it->action == syncer::ChangeRecord::ACTION_ADD) ?
          syncer::SyncChange::ACTION_ADD : syncer::SyncChange::ACTION_UPDATE;
      // Need to load specifics from node.
      syncer::ReadNode read_node(trans);
      if (read_node.InitByIdLookup(it->id) != syncer::BaseNode::INIT_OK) {
        error_handler()->OnSingleDatatypeUnrecoverableError(
            FROM_HERE,
            "Failed to look up data for received change with id " +
                base::Int64ToString(it->id));
        return;
      }
      syncer_changes_.push_back(
          syncer::SyncChange(
              FROM_HERE,
              action,
              syncer::SyncData::CreateRemoteData(
                  it->id, read_node.GetEntitySpecifics())));
    }
  }
}

void GenericChangeProcessor::CommitChangesFromSyncModel() {
  DCHECK(CalledOnValidThread());
  if (syncer_changes_.empty())
    return;
  if (!local_service_) {
    syncer::ModelType type = syncer_changes_[0].sync_data().GetDataType();
    syncer::SyncError error(FROM_HERE, "Local service destroyed.", type);
    error_handler()->OnSingleDatatypeUnrecoverableError(error.location(),
                                                        error.message());
    return;
  }
  syncer::SyncError error = local_service_->ProcessSyncChanges(FROM_HERE,
                                                       syncer_changes_);
  syncer_changes_.clear();
  if (error.IsSet()) {
    error_handler()->OnSingleDatatypeUnrecoverableError(
        error.location(), error.message());
  }
}

syncer::SyncError GenericChangeProcessor::GetSyncDataForType(
    syncer::ModelType type,
    syncer::SyncDataList* current_sync_data) {
  DCHECK(CalledOnValidThread());
  std::string type_name = syncer::ModelTypeToString(type);
  syncer::ReadTransaction trans(FROM_HERE, share_handle());
  syncer::ReadNode root(&trans);
  if (root.InitByTagLookup(syncer::ModelTypeToRootTag(type)) !=
          syncer::BaseNode::INIT_OK) {
    syncer::SyncError error(FROM_HERE,
                    "Server did not create the top-level " + type_name +
                    " node. We might be running against an out-of-date server.",
                    type);
    return error;
  }

  // TODO(akalin): We'll have to do a tree traversal for bookmarks.
  DCHECK_NE(type, syncer::BOOKMARKS);

  int64 sync_child_id = root.GetFirstChildId();
  while (sync_child_id != syncer::kInvalidId) {
    syncer::ReadNode sync_child_node(&trans);
    if (sync_child_node.InitByIdLookup(sync_child_id) !=
            syncer::BaseNode::INIT_OK) {
      syncer::SyncError error(FROM_HERE,
                      "Failed to fetch child node for type " + type_name + ".",
                       type);
      return error;
    }
    current_sync_data->push_back(syncer::SyncData::CreateRemoteData(
        sync_child_node.GetId(), sync_child_node.GetEntitySpecifics()));
    sync_child_id = sync_child_node.GetSuccessorId();
  }
  return syncer::SyncError();
}

int GenericChangeProcessor::GetSyncCountForType(syncer::ModelType type) {
  syncer::ReadTransaction trans(FROM_HERE, share_handle());
  syncer::ReadNode root(&trans);
  if (root.InitByTagLookup(syncer::ModelTypeToRootTag(type)) !=
      syncer::BaseNode::INIT_OK)
    return 0;

  // Subtract one to account for type's root node.
  return root.GetTotalNodeCount() - 1;
}

namespace {

// TODO(isherman): Investigating http://crbug.com/121592
// WARNING: this code is sensitive to compiler optimizations. Be careful
// modifying any code around an OnSingleDatatypeUnrecoverableError call, else
// the compiler attempts to merge it with other calls, losing useful information
// in breakpad uploads.
syncer::SyncError LogLookupFailure(
    syncer::BaseNode::InitByLookupResult lookup_result,
    const tracked_objects::Location& from_here,
    const std::string& error_prefix,
    syncer::ModelType type,
    DataTypeErrorHandler* error_handler) {
  switch (lookup_result) {
    case syncer::BaseNode::INIT_FAILED_ENTRY_NOT_GOOD: {
      syncer::SyncError error;
      error.Reset(from_here,
                  error_prefix +
                      "could not find entry matching the lookup criteria.",
                  type);
      error_handler->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                        error.message());
      LOG(ERROR) << "Delete: Bad entry.";
      return error;
    }
    case syncer::BaseNode::INIT_FAILED_ENTRY_IS_DEL: {
      syncer::SyncError error;
      error.Reset(from_here, error_prefix + "entry is already deleted.", type);
      error_handler->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                        error.message());
      LOG(ERROR) << "Delete: Deleted entry.";
      return error;
    }
    case syncer::BaseNode::INIT_FAILED_DECRYPT_IF_NECESSARY: {
      syncer::SyncError error;
      error.Reset(from_here, error_prefix + "unable to decrypt", type);
      error_handler->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                        error.message());
      LOG(ERROR) << "Delete: Undecryptable entry.";
      return error;
    }
    case syncer::BaseNode::INIT_FAILED_PRECONDITION: {
      syncer::SyncError error;
      error.Reset(from_here,
                  error_prefix + "a precondition was not met for calling init.",
                  type);
      error_handler->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                        error.message());
      LOG(ERROR) << "Delete: Failed precondition.";
      return error;
    }
    default: {
      syncer::SyncError error;
      // Should have listed all the possible error cases above.
      error.Reset(from_here, error_prefix + "unknown error", type);
      error_handler->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                        error.message());
      LOG(ERROR) << "Delete: Unknown error.";
      return error;
    }
  }
}

syncer::SyncError AttemptDelete(
    const syncer::SyncChange& change,
    syncer::ModelType type,
    const std::string& type_str,
    syncer::WriteNode* node,
    DataTypeErrorHandler* error_handler) {
  DCHECK_EQ(change.change_type(), syncer::SyncChange::ACTION_DELETE);
  if (change.sync_data().IsLocal()) {
    const std::string& tag = change.sync_data().GetTag();
    if (tag.empty()) {
      syncer::SyncError error(
          FROM_HERE,
          "Failed to delete " + type_str + " node. Local data, empty tag. " +
              change.location().ToString(),
          type);
      error_handler->OnSingleDatatypeUnrecoverableError(error.location(),
                                                        error.message());
      NOTREACHED();
      return error;
    }

    syncer::BaseNode::InitByLookupResult result =
        node->InitByClientTagLookup(change.sync_data().GetDataType(), tag);
    if (result != syncer::BaseNode::INIT_OK) {
      return LogLookupFailure(
          result, FROM_HERE,
          "Failed to delete " + type_str + " node. Local data. " +
              change.location().ToString(),
          type, error_handler);
    }
  } else {
    syncer::BaseNode::InitByLookupResult result =
        node->InitByIdLookup(change.sync_data().GetRemoteId());
    if (result != syncer::BaseNode::INIT_OK) {
      return LogLookupFailure(
          result, FROM_HERE,
          "Failed to delete " + type_str + " node. Non-local data. " +
              change.location().ToString(),
          type, error_handler);
    }
  }
  node->Remove();
  return syncer::SyncError();
}

}  // namespace

// WARNING: this code is sensitive to compiler optimizations. Be careful
// modifying any code around an OnSingleDatatypeUnrecoverableError call, else
// the compiler attempts to merge it with other calls, losing useful information
// in breakpad uploads.
syncer::SyncError GenericChangeProcessor::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& list_of_changes) {
  DCHECK(CalledOnValidThread());
  syncer::WriteTransaction trans(from_here, share_handle());

  for (syncer::SyncChangeList::const_iterator iter = list_of_changes.begin();
       iter != list_of_changes.end();
       ++iter) {
    const syncer::SyncChange& change = *iter;
    DCHECK_NE(change.sync_data().GetDataType(), syncer::UNSPECIFIED);
    syncer::ModelType type = change.sync_data().GetDataType();
    std::string type_str = syncer::ModelTypeToString(type);
    syncer::WriteNode sync_node(&trans);
    if (change.change_type() == syncer::SyncChange::ACTION_DELETE) {
      syncer::SyncError error =
          AttemptDelete(change, type, type_str, &sync_node,
                        error_handler());
      if (error.IsSet()) {
        NOTREACHED();
        return error;
      }
      if (merge_result_.get()) {
        merge_result_->set_num_items_deleted(
            merge_result_->num_items_deleted() + 1);
      }
    } else if (change.change_type() == syncer::SyncChange::ACTION_ADD) {
      // TODO(sync): Handle other types of creation (custom parents, folders,
      // etc.).
      syncer::ReadNode root_node(&trans);
      if (root_node.InitByTagLookup(
              syncer::ModelTypeToRootTag(change.sync_data().GetDataType())) !=
                  syncer::BaseNode::INIT_OK) {
        syncer::SyncError error(FROM_HERE,
                        "Failed to look up root node for type " + type_str,
                        type);
        error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                            error.message());
        NOTREACHED();
        LOG(ERROR) << "Create: no root node.";
        return error;
      }
      syncer::WriteNode::InitUniqueByCreationResult result =
          sync_node.InitUniqueByCreation(change.sync_data().GetDataType(),
                                          root_node,
                                          change.sync_data().GetTag());
      if (result != syncer::WriteNode::INIT_SUCCESS) {
        std::string error_prefix = "Failed to create " + type_str + " node: " +
            change.location().ToString() + ", ";
        switch (result) {
          case syncer::WriteNode::INIT_FAILED_EMPTY_TAG: {
            syncer::SyncError error;
            error.Reset(FROM_HERE, error_prefix + "empty tag", type);
            error_handler()->OnSingleDatatypeUnrecoverableError(
                FROM_HERE, error.message());
            LOG(ERROR) << "Create: Empty tag.";
            return error;
          }
          case syncer::WriteNode::INIT_FAILED_ENTRY_ALREADY_EXISTS: {
            syncer::SyncError error;
            error.Reset(FROM_HERE, error_prefix + "entry already exists", type);
            error_handler()->OnSingleDatatypeUnrecoverableError(
                FROM_HERE, error.message());
            LOG(ERROR) << "Create: Entry exists.";
            return error;
          }
          case syncer::WriteNode::INIT_FAILED_COULD_NOT_CREATE_ENTRY: {
            syncer::SyncError error;
            error.Reset(FROM_HERE, error_prefix + "failed to create entry",
                        type);
            error_handler()->OnSingleDatatypeUnrecoverableError(
                FROM_HERE, error.message());
            LOG(ERROR) << "Create: Could not create entry.";
            return error;
          }
          case syncer::WriteNode::INIT_FAILED_SET_PREDECESSOR: {
            syncer::SyncError error;
            error.Reset(FROM_HERE, error_prefix + "failed to set predecessor",
                        type);
            error_handler()->OnSingleDatatypeUnrecoverableError(
                FROM_HERE, error.message());
            LOG(ERROR) << "Create: Bad predecessor.";
            return error;
          }
          default: {
            syncer::SyncError error;
            error.Reset(FROM_HERE, error_prefix + "unknown error", type);
            error_handler()->OnSingleDatatypeUnrecoverableError(
                FROM_HERE, error.message());
            LOG(ERROR) << "Create: Unknown error.";
            return error;
          }
        }
      }
      sync_node.SetTitle(base::UTF8ToWide(change.sync_data().GetTitle()));
      sync_node.SetEntitySpecifics(change.sync_data().GetSpecifics());
      if (merge_result_.get()) {
        merge_result_->set_num_items_added(
            merge_result_->num_items_added() + 1);
      }
    } else if (change.change_type() == syncer::SyncChange::ACTION_UPDATE) {
      // TODO(zea): consider having this logic for all possible changes?
      syncer::BaseNode::InitByLookupResult result =
          sync_node.InitByClientTagLookup(change.sync_data().GetDataType(),
                                          change.sync_data().GetTag());
      if (result != syncer::BaseNode::INIT_OK) {
        std::string error_prefix = "Failed to load " + type_str + " node. " +
            change.location().ToString() + ", ";
        if (result == syncer::BaseNode::INIT_FAILED_PRECONDITION) {
          syncer::SyncError error;
          error.Reset(FROM_HERE,
                      error_prefix + "empty tag",
                      type);
          error_handler()->OnSingleDatatypeUnrecoverableError(
              FROM_HERE, error.message());
          LOG(ERROR) << "Update: Empty tag.";
          return error;
        } else if (result == syncer::BaseNode::INIT_FAILED_ENTRY_NOT_GOOD) {
          syncer::SyncError error;
          error.Reset(FROM_HERE,
                      error_prefix + "bad entry",
                      type);
          error_handler()->OnSingleDatatypeUnrecoverableError(
              FROM_HERE, error.message());
          LOG(ERROR) << "Update: bad entry.";
          return error;
        } else if (result == syncer::BaseNode::INIT_FAILED_ENTRY_IS_DEL) {
          syncer::SyncError error;
          error.Reset(FROM_HERE,
                      error_prefix + "deleted entry",
                      type);
          error_handler()->OnSingleDatatypeUnrecoverableError(
              FROM_HERE, error.message());
          LOG(ERROR) << "Update: deleted entry.";
          return error;
        } else {
          syncer::Cryptographer* crypto = trans.GetCryptographer();
          syncer::ModelTypeSet encrypted_types(trans.GetEncryptedTypes());
          const sync_pb::EntitySpecifics& specifics =
              sync_node.GetEntry()->Get(syncer::syncable::SPECIFICS);
          CHECK(specifics.has_encrypted());
          const bool can_decrypt = crypto->CanDecrypt(specifics.encrypted());
          const bool agreement = encrypted_types.Has(type);
          if (!agreement && !can_decrypt) {
            syncer::SyncError error;
            error.Reset(FROM_HERE,
                        "Failed to load encrypted entry, missing key and "
                        "nigori mismatch for " + type_str + ".",
                        type);
            error_handler()->OnSingleDatatypeUnrecoverableError(
                FROM_HERE, error.message());
            LOG(ERROR) << "Update: encr case 1.";
            return error;
          } else if (agreement && can_decrypt) {
            syncer::SyncError error;
            error.Reset(FROM_HERE,
                        "Failed to load encrypted entry, we have the key "
                        "and the nigori matches (?!) for " + type_str + ".",
                        type);
            error_handler()->OnSingleDatatypeUnrecoverableError(
                FROM_HERE, error.message());
            LOG(ERROR) << "Update: encr case 2.";
            return error;
          } else if (agreement) {
            syncer::SyncError error;
            error.Reset(FROM_HERE,
                        "Failed to load encrypted entry, missing key and "
                        "the nigori matches for " + type_str + ".",
                        type);
            error_handler()->OnSingleDatatypeUnrecoverableError(
                FROM_HERE, error.message());
            LOG(ERROR) << "Update: encr case 3.";
            return error;
          } else {
            syncer::SyncError error;
            error.Reset(FROM_HERE,
                        "Failed to load encrypted entry, we have the key"
                        "(?!) and nigori mismatch for " + type_str + ".",
                        type);
            error_handler()->OnSingleDatatypeUnrecoverableError(
                FROM_HERE, error.message());
            LOG(ERROR) << "Update: encr case 4.";
            return error;
          }
        }
      }

      sync_node.SetTitle(base::UTF8ToWide(change.sync_data().GetTitle()));
      sync_node.SetEntitySpecifics(change.sync_data().GetSpecifics());
      if (merge_result_.get()) {
        merge_result_->set_num_items_modified(
            merge_result_->num_items_modified() + 1);
      }
      // TODO(sync): Support updating other parts of the sync node (title,
      // successor, parent, etc.).
    } else {
      syncer::SyncError error(
          FROM_HERE,
          "Received unset SyncChange in the change processor, " +
              change.location().ToString(),
          type);
      error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                          error.message());
      NOTREACHED();
      LOG(ERROR) << "Unset sync change.";
      return error;
    }
  }
  return syncer::SyncError();
}

bool GenericChangeProcessor::SyncModelHasUserCreatedNodes(
    syncer::ModelType type,
    bool* has_nodes) {
  DCHECK(CalledOnValidThread());
  DCHECK(has_nodes);
  DCHECK_NE(type, syncer::UNSPECIFIED);
  std::string type_name = syncer::ModelTypeToString(type);
  std::string err_str = "Server did not create the top-level " + type_name +
      " node. We might be running against an out-of-date server.";
  *has_nodes = false;
  syncer::ReadTransaction trans(FROM_HERE, share_handle());
  syncer::ReadNode type_root_node(&trans);
  if (type_root_node.InitByTagLookup(syncer::ModelTypeToRootTag(type)) !=
          syncer::BaseNode::INIT_OK) {
    LOG(ERROR) << err_str;
    return false;
  }

  // The sync model has user created nodes if the type's root node has any
  // children.
  *has_nodes = type_root_node.HasChildren();
  return true;
}

bool GenericChangeProcessor::CryptoReadyIfNecessary(syncer::ModelType type) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(type, syncer::UNSPECIFIED);
  // We only access the cryptographer while holding a transaction.
  syncer::ReadTransaction trans(FROM_HERE, share_handle());
  const syncer::ModelTypeSet encrypted_types = trans.GetEncryptedTypes();
  return !encrypted_types.Has(type) ||
         trans.GetCryptographer()->is_ready();
}

void GenericChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(CalledOnValidThread());
}

syncer::UserShare* GenericChangeProcessor::share_handle() const {
  DCHECK(CalledOnValidThread());
  return share_handle_;
}

}  // namespace browser_sync
