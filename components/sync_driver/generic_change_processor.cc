// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/generic_change_processor.h"

#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sync_driver/sync_api_component_factory.h"
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

namespace sync_driver {

namespace {

const int kContextSizeLimit = 1024;  // Datatype context size limit.

void SetNodeSpecifics(const sync_pb::EntitySpecifics& entity_specifics,
                      syncer::WriteNode* write_node) {
  if (syncer::GetModelTypeFromSpecifics(entity_specifics) ==
          syncer::PASSWORDS) {
    write_node->SetPasswordSpecifics(
        entity_specifics.password().client_only_encrypted_data());
  } else {
    write_node->SetEntitySpecifics(entity_specifics);
  }
}

// Helper function to convert AttachmentId to AttachmentMetadataRecord.
sync_pb::AttachmentMetadataRecord AttachmentIdToRecord(
    const syncer::AttachmentId& attachment_id) {
  sync_pb::AttachmentMetadataRecord record;
  *record.mutable_id() = attachment_id.GetProto();
  return record;
}

// Replace |write_nodes|'s attachment ids with |attachment_ids|.
void SetAttachmentMetadata(const syncer::AttachmentIdList& attachment_ids,
                           syncer::WriteNode* write_node) {
  DCHECK(write_node);
  sync_pb::AttachmentMetadata attachment_metadata;
  std::transform(
      attachment_ids.begin(),
      attachment_ids.end(),
      RepeatedFieldBackInserter(attachment_metadata.mutable_record()),
      AttachmentIdToRecord);
  write_node->SetAttachmentMetadata(attachment_metadata);
}

syncer::SyncData BuildRemoteSyncData(
    int64 sync_id,
    const syncer::BaseNode& read_node,
    const syncer::AttachmentServiceProxy& attachment_service_proxy) {
  const syncer::AttachmentIdList& attachment_ids = read_node.GetAttachmentIds();
  // Use the specifics of non-password datatypes directly (encryption has
  // already been handled).
  if (read_node.GetModelType() != syncer::PASSWORDS) {
    return syncer::SyncData::CreateRemoteData(sync_id,
                                              read_node.GetEntitySpecifics(),
                                              read_node.GetModificationTime(),
                                              attachment_ids,
                                              attachment_service_proxy);
  }

  // Passwords must be accessed differently, to account for their encryption,
  // and stored into a temporary EntitySpecifics.
  sync_pb::EntitySpecifics password_holder;
  password_holder.mutable_password()->mutable_client_only_encrypted_data()->
      CopyFrom(read_node.GetPasswordSpecifics());
  return syncer::SyncData::CreateRemoteData(sync_id,
                                            password_holder,
                                            read_node.GetModificationTime(),
                                            attachment_ids,
                                            attachment_service_proxy);
}

}  // namespace

GenericChangeProcessor::GenericChangeProcessor(
    DataTypeErrorHandler* error_handler,
    const base::WeakPtr<syncer::SyncableService>& local_service,
    const base::WeakPtr<syncer::SyncMergeResult>& merge_result,
    syncer::UserShare* user_share,
    SyncApiComponentFactory* sync_factory)
    : ChangeProcessor(error_handler),
      local_service_(local_service),
      merge_result_(merge_result),
      share_handle_(user_share),
      attachment_service_(
          sync_factory->CreateAttachmentService(*user_share, this)),
      attachment_service_weak_ptr_factory_(attachment_service_.get()),
      attachment_service_proxy_(
          base::MessageLoopProxy::current(),
          attachment_service_weak_ptr_factory_.GetWeakPtr()) {
  DCHECK(CalledOnValidThread());
  DCHECK(attachment_service_);
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
      scoped_ptr<sync_pb::EntitySpecifics> specifics;
      if (it->specifics.has_password()) {
        DCHECK(it->extra.get());
        specifics.reset(new sync_pb::EntitySpecifics(it->specifics));
        specifics->mutable_password()->mutable_client_only_encrypted_data()->
            CopyFrom(it->extra->unencrypted());
      }
      const syncer::AttachmentIdList empty_list_of_attachment_ids;
      syncer_changes_.push_back(
          syncer::SyncChange(FROM_HERE,
                             syncer::SyncChange::ACTION_DELETE,
                             syncer::SyncData::CreateRemoteData(
                                 it->id,
                                 specifics ? *specifics : it->specifics,
                                 base::Time(),
                                 empty_list_of_attachment_ids,
                                 attachment_service_proxy_)));
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
      syncer_changes_.push_back(syncer::SyncChange(
          FROM_HERE,
          action,
          BuildRemoteSyncData(it->id, read_node, attachment_service_proxy_)));
    }
  }
}

void GenericChangeProcessor::CommitChangesFromSyncModel() {
  DCHECK(CalledOnValidThread());
  if (syncer_changes_.empty())
    return;
  if (!local_service_.get()) {
    syncer::ModelType type = syncer_changes_[0].sync_data().GetDataType();
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Local service destroyed.",
                            type);
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

syncer::SyncDataList GenericChangeProcessor::GetAllSyncData(
    syncer::ModelType type) const {
  // This is slow / memory intensive.  Should be used sparingly by datatypes.
  syncer::SyncDataList data;
  GetAllSyncDataReturnError(type, &data);
  return data;
}

syncer::SyncError GenericChangeProcessor::UpdateDataTypeContext(
    syncer::ModelType type,
    syncer::SyncChangeProcessor::ContextRefreshStatus refresh_status,
    const std::string& context) {
  DCHECK(syncer::ProtocolTypes().Has(type));

  if (context.size() > static_cast<size_t>(kContextSizeLimit)) {
    return syncer::SyncError(FROM_HERE,
                             syncer::SyncError::DATATYPE_ERROR,
                             "Context size limit exceeded.",
                             type);
  }

  syncer::WriteTransaction trans(FROM_HERE, share_handle());
  trans.SetDataTypeContext(type, refresh_status, context);

  // TODO(zea): plumb a pointer to the PSS or SyncManagerImpl here so we can
  // trigger a datatype nudge if |refresh_status == REFRESH_NEEDED|.

  return syncer::SyncError();
}

void GenericChangeProcessor::OnAttachmentUploaded(
    const syncer::AttachmentId& attachment_id) {
  syncer::WriteTransaction trans(FROM_HERE, share_handle());
  trans.UpdateEntriesMarkAttachmentAsOnServer(attachment_id);
}

syncer::SyncError GenericChangeProcessor::GetAllSyncDataReturnError(
    syncer::ModelType type,
    syncer::SyncDataList* current_sync_data) const {
  DCHECK(CalledOnValidThread());
  std::string type_name = syncer::ModelTypeToString(type);
  syncer::ReadTransaction trans(FROM_HERE, share_handle());
  syncer::ReadNode root(&trans);
  if (root.InitTypeRoot(type) != syncer::BaseNode::INIT_OK) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Server did not create the top-level " + type_name +
                                " node. We might be running against an out-of-"
                                "date server.",
                            type);
    return error;
  }

  // TODO(akalin): We'll have to do a tree traversal for bookmarks.
  DCHECK_NE(type, syncer::BOOKMARKS);

  std::vector<int64> child_ids;
  root.GetChildIds(&child_ids);

  for (std::vector<int64>::iterator it = child_ids.begin();
       it != child_ids.end(); ++it) {
    syncer::ReadNode sync_child_node(&trans);
    if (sync_child_node.InitByIdLookup(*it) !=
            syncer::BaseNode::INIT_OK) {
      syncer::SyncError error(FROM_HERE,
                              syncer::SyncError::DATATYPE_ERROR,
                              "Failed to fetch child node for type " +
                                  type_name + ".",
                              type);
      return error;
    }
    current_sync_data->push_back(BuildRemoteSyncData(
        sync_child_node.GetId(), sync_child_node, attachment_service_proxy_));
  }
  return syncer::SyncError();
}

bool GenericChangeProcessor::GetDataTypeContext(syncer::ModelType type,
                                                std::string* context) const {
  syncer::ReadTransaction trans(FROM_HERE, share_handle());
  sync_pb::DataTypeContext context_proto;
  trans.GetDataTypeContext(type, &context_proto);
  if (!context_proto.has_context())
    return false;

  DCHECK_EQ(type,
            syncer::GetModelTypeFromSpecificsFieldNumber(
                context_proto.data_type_id()));
  *context = context_proto.context();
  return true;
}

int GenericChangeProcessor::GetSyncCountForType(syncer::ModelType type) {
  syncer::ReadTransaction trans(FROM_HERE, share_handle());
  syncer::ReadNode root(&trans);
  if (root.InitTypeRoot(type) != syncer::BaseNode::INIT_OK)
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

syncer::SyncError AttemptDelete(const syncer::SyncChange& change,
                                syncer::ModelType type,
                                const std::string& type_str,
                                syncer::WriteNode* node,
                                DataTypeErrorHandler* error_handler) {
  DCHECK_EQ(change.change_type(), syncer::SyncChange::ACTION_DELETE);
  if (change.sync_data().IsLocal()) {
    const std::string& tag = syncer::SyncDataLocal(change.sync_data()).GetTag();
    if (tag.empty()) {
      syncer::SyncError error(
          FROM_HERE,
          syncer::SyncError::DATATYPE_ERROR,
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
    syncer::BaseNode::InitByLookupResult result = node->InitByIdLookup(
        syncer::SyncDataRemote(change.sync_data()).GetId());
    if (result != syncer::BaseNode::INIT_OK) {
      return LogLookupFailure(
          result, FROM_HERE,
          "Failed to delete " + type_str + " node. Non-local data. " +
              change.location().ToString(),
          type, error_handler);
    }
  }
  if (IsActOnceDataType(type))
    node->Drop();
  else
    node->Tombstone();
  return syncer::SyncError();
}

// A callback invoked on completion of AttachmentService::StoreAttachment.
void IgnoreStoreResult(const syncer::AttachmentService::StoreResult&) {
  // TODO(maniscalco): Here is where we're going to update the in-directory
  // entry to indicate that the attachments have been successfully stored on
  // disk.  Why do we care?  Because we might crash after persisting the
  // directory to disk, but before we have persisted its attachments, leaving us
  // with danging attachment ids.  Having a flag that indicates we've stored the
  // entry will allow us to detect and filter entries with dangling attachment
  // ids (bug 368353).
}

void StoreAttachments(syncer::AttachmentService* attachment_service,
                      const syncer::AttachmentList& attachments) {
  DCHECK(attachment_service);
  syncer::AttachmentService::StoreCallback ignore_store_result =
      base::Bind(&IgnoreStoreResult);
  attachment_service->StoreAttachments(attachments, ignore_store_result);
}

}  // namespace

syncer::SyncError GenericChangeProcessor::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& list_of_changes) {
  DCHECK(CalledOnValidThread());

  // Keep track of brand new attachments so we can persist them on this device
  // and upload them to the server.
  syncer::AttachmentList new_attachments;

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
          AttemptDelete(change, type, type_str, &sync_node, error_handler());
      if (error.IsSet()) {
        NOTREACHED();
        return error;
      }
      if (merge_result_.get()) {
        merge_result_->set_num_items_deleted(
            merge_result_->num_items_deleted() + 1);
      }
    } else if (change.change_type() == syncer::SyncChange::ACTION_ADD) {
      syncer::SyncError error = HandleActionAdd(
          change, type_str, type, trans, &sync_node, &new_attachments);
      if (error.IsSet()) {
        return error;
      }
    } else if (change.change_type() == syncer::SyncChange::ACTION_UPDATE) {
      syncer::SyncError error = HandleActionUpdate(
          change, type_str, type, trans, &sync_node, &new_attachments);
      if (error.IsSet()) {
        return error;
      }
    } else {
      syncer::SyncError error(
          FROM_HERE,
          syncer::SyncError::DATATYPE_ERROR,
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

  if (!new_attachments.empty()) {
    StoreAttachments(attachment_service_.get(), new_attachments);
  }

  return syncer::SyncError();
}

// WARNING: this code is sensitive to compiler optimizations. Be careful
// modifying any code around an OnSingleDatatypeUnrecoverableError call, else
// the compiler attempts to merge it with other calls, losing useful information
// in breakpad uploads.
syncer::SyncError GenericChangeProcessor::HandleActionAdd(
    const syncer::SyncChange& change,
    const std::string& type_str,
    const syncer::ModelType& type,
    const syncer::WriteTransaction& trans,
    syncer::WriteNode* sync_node,
    syncer::AttachmentList* new_attachments) {
  // TODO(sync): Handle other types of creation (custom parents, folders,
  // etc.).
  syncer::ReadNode root_node(&trans);
  const syncer::SyncDataLocal sync_data_local(change.sync_data());
  if (root_node.InitTypeRoot(sync_data_local.GetDataType()) !=
      syncer::BaseNode::INIT_OK) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Failed to look up root node for type " + type_str,
                            type);
    error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                        error.message());
    NOTREACHED();
    LOG(ERROR) << "Create: no root node.";
    return error;
  }
  syncer::WriteNode::InitUniqueByCreationResult result =
      sync_node->InitUniqueByCreation(
          sync_data_local.GetDataType(), root_node, sync_data_local.GetTag());
  if (result != syncer::WriteNode::INIT_SUCCESS) {
    std::string error_prefix = "Failed to create " + type_str + " node: " +
                               change.location().ToString() + ", ";
    switch (result) {
      case syncer::WriteNode::INIT_FAILED_EMPTY_TAG: {
        syncer::SyncError error;
        error.Reset(FROM_HERE, error_prefix + "empty tag", type);
        error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                            error.message());
        LOG(ERROR) << "Create: Empty tag.";
        return error;
      }
      case syncer::WriteNode::INIT_FAILED_ENTRY_ALREADY_EXISTS: {
        syncer::SyncError error;
        error.Reset(FROM_HERE, error_prefix + "entry already exists", type);
        error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                            error.message());
        LOG(ERROR) << "Create: Entry exists.";
        return error;
      }
      case syncer::WriteNode::INIT_FAILED_COULD_NOT_CREATE_ENTRY: {
        syncer::SyncError error;
        error.Reset(FROM_HERE, error_prefix + "failed to create entry", type);
        error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                            error.message());
        LOG(ERROR) << "Create: Could not create entry.";
        return error;
      }
      case syncer::WriteNode::INIT_FAILED_SET_PREDECESSOR: {
        syncer::SyncError error;
        error.Reset(
            FROM_HERE, error_prefix + "failed to set predecessor", type);
        error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                            error.message());
        LOG(ERROR) << "Create: Bad predecessor.";
        return error;
      }
      default: {
        syncer::SyncError error;
        error.Reset(FROM_HERE, error_prefix + "unknown error", type);
        error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                            error.message());
        LOG(ERROR) << "Create: Unknown error.";
        return error;
      }
    }
  }
  sync_node->SetTitle(change.sync_data().GetTitle());
  SetNodeSpecifics(sync_data_local.GetSpecifics(), sync_node);

  syncer::AttachmentIdList attachment_ids = sync_data_local.GetAttachmentIds();
  SetAttachmentMetadata(attachment_ids, sync_node);

  // Return any newly added attachments.
  const syncer::AttachmentList& local_attachments_for_upload =
      sync_data_local.GetLocalAttachmentsForUpload();
  new_attachments->insert(new_attachments->end(),
                          local_attachments_for_upload.begin(),
                          local_attachments_for_upload.end());

  if (merge_result_.get()) {
    merge_result_->set_num_items_added(merge_result_->num_items_added() + 1);
  }
  return syncer::SyncError();
}
// WARNING: this code is sensitive to compiler optimizations. Be careful
// modifying any code around an OnSingleDatatypeUnrecoverableError call, else
// the compiler attempts to merge it with other calls, losing useful information
// in breakpad uploads.
syncer::SyncError GenericChangeProcessor::HandleActionUpdate(
    const syncer::SyncChange& change,
    const std::string& type_str,
    const syncer::ModelType& type,
    const syncer::WriteTransaction& trans,
    syncer::WriteNode* sync_node,
    syncer::AttachmentList* new_attachments) {
  // TODO(zea): consider having this logic for all possible changes?

  const syncer::SyncDataLocal sync_data_local(change.sync_data());
  syncer::BaseNode::InitByLookupResult result =
      sync_node->InitByClientTagLookup(sync_data_local.GetDataType(),
                                       sync_data_local.GetTag());
  if (result != syncer::BaseNode::INIT_OK) {
    std::string error_prefix = "Failed to load " + type_str + " node. " +
                               change.location().ToString() + ", ";
    if (result == syncer::BaseNode::INIT_FAILED_PRECONDITION) {
      syncer::SyncError error;
      error.Reset(FROM_HERE, error_prefix + "empty tag", type);
      error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                          error.message());
      LOG(ERROR) << "Update: Empty tag.";
      return error;
    } else if (result == syncer::BaseNode::INIT_FAILED_ENTRY_NOT_GOOD) {
      syncer::SyncError error;
      error.Reset(FROM_HERE, error_prefix + "bad entry", type);
      error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                          error.message());
      LOG(ERROR) << "Update: bad entry.";
      return error;
    } else if (result == syncer::BaseNode::INIT_FAILED_ENTRY_IS_DEL) {
      syncer::SyncError error;
      error.Reset(FROM_HERE, error_prefix + "deleted entry", type);
      error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                          error.message());
      LOG(ERROR) << "Update: deleted entry.";
      return error;
    } else {
      syncer::Cryptographer* crypto = trans.GetCryptographer();
      syncer::ModelTypeSet encrypted_types(trans.GetEncryptedTypes());
      const sync_pb::EntitySpecifics& specifics =
          sync_node->GetEntry()->GetSpecifics();
      CHECK(specifics.has_encrypted());
      const bool can_decrypt = crypto->CanDecrypt(specifics.encrypted());
      const bool agreement = encrypted_types.Has(type);
      if (!agreement && !can_decrypt) {
        syncer::SyncError error;
        error.Reset(FROM_HERE,
                    "Failed to load encrypted entry, missing key and "
                    "nigori mismatch for " +
                        type_str + ".",
                    type);
        error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                            error.message());
        LOG(ERROR) << "Update: encr case 1.";
        return error;
      } else if (agreement && can_decrypt) {
        syncer::SyncError error;
        error.Reset(FROM_HERE,
                    "Failed to load encrypted entry, we have the key "
                    "and the nigori matches (?!) for " +
                        type_str + ".",
                    type);
        error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                            error.message());
        LOG(ERROR) << "Update: encr case 2.";
        return error;
      } else if (agreement) {
        syncer::SyncError error;
        error.Reset(FROM_HERE,
                    "Failed to load encrypted entry, missing key and "
                    "the nigori matches for " +
                        type_str + ".",
                    type);
        error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                            error.message());
        LOG(ERROR) << "Update: encr case 3.";
        return error;
      } else {
        syncer::SyncError error;
        error.Reset(FROM_HERE,
                    "Failed to load encrypted entry, we have the key"
                    "(?!) and nigori mismatch for " +
                        type_str + ".",
                    type);
        error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                                            error.message());
        LOG(ERROR) << "Update: encr case 4.";
        return error;
      }
    }
  }

  sync_node->SetTitle(change.sync_data().GetTitle());
  SetNodeSpecifics(sync_data_local.GetSpecifics(), sync_node);
  SetAttachmentMetadata(sync_data_local.GetAttachmentIds(), sync_node);

  // Return any newly added attachments.
  const syncer::AttachmentList& local_attachments_for_upload =
      sync_data_local.GetLocalAttachmentsForUpload();
  new_attachments->insert(new_attachments->end(),
                          local_attachments_for_upload.begin(),
                          local_attachments_for_upload.end());

  if (merge_result_.get()) {
    merge_result_->set_num_items_modified(merge_result_->num_items_modified() +
                                          1);
  }
  // TODO(sync): Support updating other parts of the sync node (title,
  // successor, parent, etc.).
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
  if (type_root_node.InitTypeRoot(type) != syncer::BaseNode::INIT_OK) {
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

void GenericChangeProcessor::StartImpl() {
}

syncer::UserShare* GenericChangeProcessor::share_handle() const {
  DCHECK(CalledOnValidThread());
  return share_handle_;
}

}  // namespace sync_driver
