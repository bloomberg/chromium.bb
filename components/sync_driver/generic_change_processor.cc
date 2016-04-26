// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/generic_change_processor.h"

#include <stddef.h>
#include <algorithm>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "components/sync_driver/sync_api_component_factory.h"
#include "components/sync_driver/sync_client.h"
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
    int64_t sync_id,
    const syncer::ReadNode& read_node,
    const syncer::AttachmentServiceProxy& attachment_service_proxy) {
  const syncer::AttachmentIdList& attachment_ids = read_node.GetAttachmentIds();
  switch (read_node.GetModelType()) {
    case syncer::PASSWORDS: {
      // Passwords must be accessed differently, to account for their
      // encryption, and stored into a temporary EntitySpecifics.
      sync_pb::EntitySpecifics password_holder;
      password_holder.mutable_password()
          ->mutable_client_only_encrypted_data()
          ->CopyFrom(read_node.GetPasswordSpecifics());
      return syncer::SyncData::CreateRemoteData(
          sync_id, password_holder, read_node.GetModificationTime(),
          attachment_ids, attachment_service_proxy);
    }
    case syncer::SESSIONS:
      // Include tag hashes for sessions data type to allow discarding during
      // merge if re-hashing by the service gives a different value. This is to
      // allow removal of incorrectly hashed values, see crbug.com/604657. This
      // cannot be done in the processor because only the service knows how to
      // generate a tag from the specifics. We don't set this value for other
      // data types because they shouldn't need it and it costs memory to hold
      // another copy of this string around.
      return syncer::SyncData::CreateRemoteData(
          sync_id, read_node.GetEntitySpecifics(),
          read_node.GetModificationTime(), attachment_ids,
          attachment_service_proxy, read_node.GetEntry()->GetUniqueClientTag());
    default:
      // Use the specifics directly, encryption has already been handled.
      return syncer::SyncData::CreateRemoteData(
          sync_id, read_node.GetEntitySpecifics(),
          read_node.GetModificationTime(), attachment_ids,
          attachment_service_proxy);
  }
}

}  // namespace

GenericChangeProcessor::GenericChangeProcessor(
    syncer::ModelType type,
    DataTypeErrorHandler* error_handler,
    const base::WeakPtr<syncer::SyncableService>& local_service,
    const base::WeakPtr<syncer::SyncMergeResult>& merge_result,
    syncer::UserShare* user_share,
    SyncClient* sync_client,
    std::unique_ptr<syncer::AttachmentStoreForSync> attachment_store)
    : ChangeProcessor(error_handler),
      type_(type),
      local_service_(local_service),
      merge_result_(merge_result),
      share_handle_(user_share),
      weak_ptr_factory_(this) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(type_, syncer::UNSPECIFIED);
  if (attachment_store) {
    std::string store_birthday;
    {
      syncer::ReadTransaction trans(FROM_HERE, share_handle());
      store_birthday = trans.GetStoreBirthday();
    }
    attachment_service_ =
        sync_client->GetSyncApiComponentFactory()->CreateAttachmentService(
            std::move(attachment_store), *user_share, store_birthday, type,
            this);
    attachment_service_weak_ptr_factory_.reset(
        new base::WeakPtrFactory<syncer::AttachmentService>(
            attachment_service_.get()));
    attachment_service_proxy_ = syncer::AttachmentServiceProxy(
        base::ThreadTaskRunnerHandle::Get(),
        attachment_service_weak_ptr_factory_->GetWeakPtr());
    UploadAllAttachmentsNotOnServer();
  } else {
    attachment_service_proxy_ = syncer::AttachmentServiceProxy(
        base::ThreadTaskRunnerHandle::Get(),
        base::WeakPtr<syncer::AttachmentService>());
  }
}

GenericChangeProcessor::~GenericChangeProcessor() {
  DCHECK(CalledOnValidThread());
}

void GenericChangeProcessor::ApplyChangesFromSyncModel(
    const syncer::BaseTransaction* trans,
    int64_t model_version,
    const syncer::ImmutableChangeRecordList& changes) {
  DCHECK(CalledOnValidThread());
  DCHECK(syncer_changes_.empty());
  for (syncer::ChangeRecordList::const_iterator it =
           changes.Get().begin(); it != changes.Get().end(); ++it) {
    if (it->action == syncer::ChangeRecord::ACTION_DELETE) {
      std::unique_ptr<sync_pb::EntitySpecifics> specifics;
      if (it->specifics.has_password()) {
        DCHECK(it->extra.get());
        specifics.reset(new sync_pb::EntitySpecifics(it->specifics));
        specifics->mutable_password()->mutable_client_only_encrypted_data()->
            CopyFrom(it->extra->unencrypted());
      }
      const syncer::AttachmentIdList empty_list_of_attachment_ids;
      syncer_changes_.push_back(syncer::SyncChange(
          FROM_HERE, syncer::SyncChange::ACTION_DELETE,
          syncer::SyncData::CreateRemoteData(
              it->id, specifics ? *specifics : it->specifics, base::Time(),
              empty_list_of_attachment_ids, attachment_service_proxy_)));
    } else {
      syncer::SyncChange::SyncChangeType action =
          (it->action == syncer::ChangeRecord::ACTION_ADD) ?
          syncer::SyncChange::ACTION_ADD : syncer::SyncChange::ACTION_UPDATE;
      // Need to load specifics from node.
      syncer::ReadNode read_node(trans);
      if (read_node.InitByIdLookup(it->id) != syncer::BaseNode::INIT_OK) {
        syncer::SyncError error(
            FROM_HERE,
            syncer::SyncError::DATATYPE_ERROR,
            "Failed to look up data for received change with id " +
                base::Int64ToString(it->id),
            syncer::GetModelTypeFromSpecifics(it->specifics));
        error_handler()->OnSingleDataTypeUnrecoverableError(error);
        return;
      }
      syncer_changes_.push_back(syncer::SyncChange(
          FROM_HERE, action,
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
    error_handler()->OnSingleDataTypeUnrecoverableError(error);
    return;
  }
  syncer::SyncError error = local_service_->ProcessSyncChanges(FROM_HERE,
                                                       syncer_changes_);
  syncer_changes_.clear();
  if (error.IsSet())
    error_handler()->OnSingleDataTypeUnrecoverableError(error);
}

syncer::SyncDataList GenericChangeProcessor::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_EQ(type_, type);
  // This is slow / memory intensive.  Should be used sparingly by datatypes.
  syncer::SyncDataList data;
  GetAllSyncDataReturnError(&data);
  return data;
}

syncer::SyncError GenericChangeProcessor::UpdateDataTypeContext(
    syncer::ModelType type,
    syncer::SyncChangeProcessor::ContextRefreshStatus refresh_status,
    const std::string& context) {
  DCHECK(syncer::ProtocolTypes().Has(type));
  DCHECK_EQ(type_, type);

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
    syncer::SyncDataList* current_sync_data) const {
  DCHECK(CalledOnValidThread());
  std::string type_name = syncer::ModelTypeToString(type_);
  syncer::ReadTransaction trans(FROM_HERE, share_handle());
  syncer::ReadNode root(&trans);
  if (root.InitTypeRoot(type_) != syncer::BaseNode::INIT_OK) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Server did not create the top-level " + type_name +
                                " node. We might be running against an out-of-"
                                "date server.",
                            type_);
    return error;
  }

  // TODO(akalin): We'll have to do a tree traversal for bookmarks.
  DCHECK_NE(type_, syncer::BOOKMARKS);

  std::vector<int64_t> child_ids;
  root.GetChildIds(&child_ids);

  for (std::vector<int64_t>::iterator it = child_ids.begin();
       it != child_ids.end(); ++it) {
    syncer::ReadNode sync_child_node(&trans);
    if (sync_child_node.InitByIdLookup(*it) !=
            syncer::BaseNode::INIT_OK) {
      syncer::SyncError error(
          FROM_HERE,
          syncer::SyncError::DATATYPE_ERROR,
          "Failed to fetch child node for type " + type_name + ".",
          type_);
      return error;
    }
    current_sync_data->push_back(BuildRemoteSyncData(
        sync_child_node.GetId(), sync_child_node, attachment_service_proxy_));
  }
  return syncer::SyncError();
}

bool GenericChangeProcessor::GetDataTypeContext(std::string* context) const {
  syncer::ReadTransaction trans(FROM_HERE, share_handle());
  sync_pb::DataTypeContext context_proto;
  trans.GetDataTypeContext(type_, &context_proto);
  if (!context_proto.has_context())
    return false;

  DCHECK_EQ(type_,
            syncer::GetModelTypeFromSpecificsFieldNumber(
                context_proto.data_type_id()));
  *context = context_proto.context();
  return true;
}

int GenericChangeProcessor::GetSyncCount() {
  syncer::ReadTransaction trans(FROM_HERE, share_handle());
  syncer::ReadNode root(&trans);
  if (root.InitTypeRoot(type_) != syncer::BaseNode::INIT_OK)
    return 0;

  // Subtract one to account for type's root node.
  return root.GetTotalNodeCount() - 1;
}

namespace {

// WARNING: this code is sensitive to compiler optimizations. Be careful
// modifying any code around an OnSingleDataTypeUnrecoverableError call, else
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
      error_handler->OnSingleDataTypeUnrecoverableError(error);
      LOG(ERROR) << "Delete: Bad entry.";
      return error;
    }
    case syncer::BaseNode::INIT_FAILED_ENTRY_IS_DEL: {
      syncer::SyncError error;
      error.Reset(from_here, error_prefix + "entry is already deleted.", type);
      error_handler->OnSingleDataTypeUnrecoverableError(error);
      LOG(ERROR) << "Delete: Deleted entry.";
      return error;
    }
    case syncer::BaseNode::INIT_FAILED_DECRYPT_IF_NECESSARY: {
      syncer::SyncError error;
      error.Reset(from_here, error_prefix + "unable to decrypt", type);
      error_handler->OnSingleDataTypeUnrecoverableError(error);
      LOG(ERROR) << "Delete: Undecryptable entry.";
      return error;
    }
    case syncer::BaseNode::INIT_FAILED_PRECONDITION: {
      syncer::SyncError error;
      error.Reset(from_here,
                  error_prefix + "a precondition was not met for calling init.",
                  type);
      error_handler->OnSingleDataTypeUnrecoverableError(error);
      LOG(ERROR) << "Delete: Failed precondition.";
      return error;
    }
    default: {
      syncer::SyncError error;
      // Should have listed all the possible error cases above.
      error.Reset(from_here, error_prefix + "unknown error", type);
      error_handler->OnSingleDataTypeUnrecoverableError(error);
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
      error_handler->OnSingleDataTypeUnrecoverableError(error);
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

}  // namespace

syncer::SyncError GenericChangeProcessor::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& list_of_changes) {
  DCHECK(CalledOnValidThread());

  if (list_of_changes.empty()) {
    // No work. Exit without entering WriteTransaction.
    return syncer::SyncError();
  }

  // Keep track of brand new attachments so we can persist them on this device
  // and upload them to the server.
  syncer::AttachmentIdSet new_attachments;

  syncer::WriteTransaction trans(from_here, share_handle());

  for (syncer::SyncChangeList::const_iterator iter = list_of_changes.begin();
       iter != list_of_changes.end();
       ++iter) {
    const syncer::SyncChange& change = *iter;
    DCHECK_EQ(change.sync_data().GetDataType(), type_);
    std::string type_str = syncer::ModelTypeToString(type_);
    syncer::WriteNode sync_node(&trans);
    if (change.change_type() == syncer::SyncChange::ACTION_DELETE) {
      syncer::SyncError error =
          AttemptDelete(change, type_, type_str, &sync_node, error_handler());
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
          change, type_str, trans, &sync_node, &new_attachments);
      if (error.IsSet()) {
        return error;
      }
    } else if (change.change_type() == syncer::SyncChange::ACTION_UPDATE) {
      syncer::SyncError error = HandleActionUpdate(
          change, type_str, trans, &sync_node, &new_attachments);
      if (error.IsSet()) {
        return error;
      }
    } else {
      syncer::SyncError error(
          FROM_HERE,
          syncer::SyncError::DATATYPE_ERROR,
          "Received unset SyncChange in the change processor, " +
              change.location().ToString(),
          type_);
      error_handler()->OnSingleDataTypeUnrecoverableError(error);
      NOTREACHED();
      LOG(ERROR) << "Unset sync change.";
      return error;
    }
  }

  if (!new_attachments.empty()) {
    // If datatype uses attachments it should have supplied attachment store
    // which would initialize attachment_service_. Fail if it isn't so.
    if (!attachment_service_.get()) {
      syncer::SyncError error(
          FROM_HERE,
          syncer::SyncError::DATATYPE_ERROR,
          "Datatype performs attachment operation without initializing "
          "attachment store",
          type_);
      error_handler()->OnSingleDataTypeUnrecoverableError(error);
      NOTREACHED();
      return error;
    }
    syncer::AttachmentIdList ids_to_upload;
    ids_to_upload.reserve(new_attachments.size());
    std::copy(new_attachments.begin(), new_attachments.end(),
              std::back_inserter(ids_to_upload));
    attachment_service_->UploadAttachments(ids_to_upload);
  }

  return syncer::SyncError();
}

// WARNING: this code is sensitive to compiler optimizations. Be careful
// modifying any code around an OnSingleDataTypeUnrecoverableError call, else
// the compiler attempts to merge it with other calls, losing useful information
// in breakpad uploads.
syncer::SyncError GenericChangeProcessor::HandleActionAdd(
    const syncer::SyncChange& change,
    const std::string& type_str,
    const syncer::WriteTransaction& trans,
    syncer::WriteNode* sync_node,
    syncer::AttachmentIdSet* new_attachments) {
  // TODO(sync): Handle other types of creation (custom parents, folders,
  // etc.).
  const syncer::SyncDataLocal sync_data_local(change.sync_data());
  syncer::WriteNode::InitUniqueByCreationResult result =
      sync_node->InitUniqueByCreation(sync_data_local.GetDataType(),
                                      sync_data_local.GetTag());
  if (result != syncer::WriteNode::INIT_SUCCESS) {
    std::string error_prefix = "Failed to create " + type_str + " node: " +
                               change.location().ToString() + ", ";
    switch (result) {
      case syncer::WriteNode::INIT_FAILED_EMPTY_TAG: {
        syncer::SyncError error;
        error.Reset(FROM_HERE, error_prefix + "empty tag", type_);
        error_handler()->OnSingleDataTypeUnrecoverableError(error);
        LOG(ERROR) << "Create: Empty tag.";
        return error;
      }
      case syncer::WriteNode::INIT_FAILED_COULD_NOT_CREATE_ENTRY: {
        syncer::SyncError error;
        error.Reset(FROM_HERE, error_prefix + "failed to create entry", type_);
        error_handler()->OnSingleDataTypeUnrecoverableError(error);
        LOG(ERROR) << "Create: Could not create entry.";
        return error;
      }
      case syncer::WriteNode::INIT_FAILED_SET_PREDECESSOR: {
        syncer::SyncError error;
        error.Reset(
            FROM_HERE, error_prefix + "failed to set predecessor", type_);
        error_handler()->OnSingleDataTypeUnrecoverableError(error);
        LOG(ERROR) << "Create: Bad predecessor.";
        return error;
      }
      case syncer::WriteNode::INIT_FAILED_DECRYPT_EXISTING_ENTRY: {
        syncer::SyncError error;
        error.Reset(FROM_HERE, error_prefix + "failed to decrypt", type_);
        error_handler()->OnSingleDataTypeUnrecoverableError(error);
        LOG(ERROR) << "Create: Failed to decrypt.";
        return error;
      }
      default: {
        syncer::SyncError error;
        error.Reset(FROM_HERE, error_prefix + "unknown error", type_);
        error_handler()->OnSingleDataTypeUnrecoverableError(error);
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
  new_attachments->insert(attachment_ids.begin(), attachment_ids.end());
  if (merge_result_.get()) {
    merge_result_->set_num_items_added(merge_result_->num_items_added() + 1);
  }
  return syncer::SyncError();
}
// WARNING: this code is sensitive to compiler optimizations. Be careful
// modifying any code around an OnSingleDataTypeUnrecoverableError call, else
// the compiler attempts to merge it with other calls, losing useful information
// in breakpad uploads.
syncer::SyncError GenericChangeProcessor::HandleActionUpdate(
    const syncer::SyncChange& change,
    const std::string& type_str,
    const syncer::WriteTransaction& trans,
    syncer::WriteNode* sync_node,
    syncer::AttachmentIdSet* new_attachments) {
  const syncer::SyncDataLocal sync_data_local(change.sync_data());
  syncer::BaseNode::InitByLookupResult result =
      sync_node->InitByClientTagLookup(sync_data_local.GetDataType(),
                                       sync_data_local.GetTag());
  if (result != syncer::BaseNode::INIT_OK) {
    std::string error_prefix = "Failed to load " + type_str + " node. " +
                               change.location().ToString() + ", ";
    if (result == syncer::BaseNode::INIT_FAILED_PRECONDITION) {
      syncer::SyncError error;
      error.Reset(FROM_HERE, error_prefix + "empty tag", type_);
      error_handler()->OnSingleDataTypeUnrecoverableError(error);
      LOG(ERROR) << "Update: Empty tag.";
      return error;
    } else if (result == syncer::BaseNode::INIT_FAILED_ENTRY_NOT_GOOD) {
      syncer::SyncError error;
      error.Reset(FROM_HERE, error_prefix + "bad entry", type_);
      error_handler()->OnSingleDataTypeUnrecoverableError(error);
      LOG(ERROR) << "Update: bad entry.";
      return error;
    } else if (result == syncer::BaseNode::INIT_FAILED_ENTRY_IS_DEL) {
      syncer::SyncError error;
      error.Reset(FROM_HERE, error_prefix + "deleted entry", type_);
      error_handler()->OnSingleDataTypeUnrecoverableError(error);
      LOG(ERROR) << "Update: deleted entry.";
      return error;
    } else if (result == syncer::BaseNode::INIT_FAILED_DECRYPT_IF_NECESSARY) {
      syncer::SyncError error;
      error.Reset(FROM_HERE, error_prefix + "failed to decrypt", type_);
      error_handler()->OnSingleDataTypeUnrecoverableError(error);
      LOG(ERROR) << "Update: Failed to decrypt.";
      return error;
    } else {
      NOTREACHED();
      syncer::SyncError error;
      error.Reset(FROM_HERE, error_prefix + "unknown error", type_);
      error_handler()->OnSingleDataTypeUnrecoverableError(error);
      LOG(ERROR) << "Update: Unknown error.";
      return error;
    }
  }

  sync_node->SetTitle(change.sync_data().GetTitle());
  SetNodeSpecifics(sync_data_local.GetSpecifics(), sync_node);
  syncer::AttachmentIdList attachment_ids = sync_data_local.GetAttachmentIds();
  SetAttachmentMetadata(attachment_ids, sync_node);

  // Return any newly added attachments.
  new_attachments->insert(attachment_ids.begin(), attachment_ids.end());

  if (merge_result_.get()) {
    merge_result_->set_num_items_modified(merge_result_->num_items_modified() +
                                          1);
  }
  // TODO(sync): Support updating other parts of the sync node (title,
  // successor, parent, etc.).
  return syncer::SyncError();
}

bool GenericChangeProcessor::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(CalledOnValidThread());
  DCHECK(has_nodes);
  std::string type_name = syncer::ModelTypeToString(type_);
  std::string err_str = "Server did not create the top-level " + type_name +
      " node. We might be running against an out-of-date server.";
  *has_nodes = false;
  syncer::ReadTransaction trans(FROM_HERE, share_handle());
  syncer::ReadNode type_root_node(&trans);
  if (type_root_node.InitTypeRoot(type_) != syncer::BaseNode::INIT_OK) {
    LOG(ERROR) << err_str;
    return false;
  }

  // The sync model has user created nodes if the type's root node has any
  // children.
  *has_nodes = type_root_node.HasChildren();
  return true;
}

bool GenericChangeProcessor::CryptoReadyIfNecessary() {
  DCHECK(CalledOnValidThread());
  // We only access the cryptographer while holding a transaction.
  syncer::ReadTransaction trans(FROM_HERE, share_handle());
  const syncer::ModelTypeSet encrypted_types = trans.GetEncryptedTypes();
  return !encrypted_types.Has(type_) || trans.GetCryptographer()->is_ready();
}

void GenericChangeProcessor::StartImpl() {
}

syncer::UserShare* GenericChangeProcessor::share_handle() const {
  DCHECK(CalledOnValidThread());
  return share_handle_;
}

void GenericChangeProcessor::UploadAllAttachmentsNotOnServer() {
  DCHECK(CalledOnValidThread());
  DCHECK(attachment_service_.get());
  syncer::AttachmentIdList ids;
  {
    syncer::ReadTransaction trans(FROM_HERE, share_handle());
    trans.GetAttachmentIdsToUpload(type_, &ids);
  }
  if (!ids.empty()) {
    attachment_service_->UploadAttachments(ids);
  }
}

std::unique_ptr<syncer::AttachmentService>
GenericChangeProcessor::GetAttachmentService() const {
  return std::unique_ptr<syncer::AttachmentService>(
      new syncer::AttachmentServiceProxy(attachment_service_proxy_));
}

}  // namespace sync_driver
