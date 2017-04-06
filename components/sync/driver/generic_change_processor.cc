// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/generic_change_processor.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/base/unrecoverable_error_handler.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/model/local_change_observer.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/syncable/base_node.h"
#include "components/sync/syncable/change_record.h"
#include "components/sync/syncable/entry.h"  // TODO(tim): Bug 123674.
#include "components/sync/syncable/read_node.h"
#include "components/sync/syncable/read_transaction.h"
#include "components/sync/syncable/write_node.h"
#include "components/sync/syncable/write_transaction.h"

namespace syncer {

namespace {

const int kContextSizeLimit = 1024;  // Datatype context size limit.

void SetNodeSpecifics(const sync_pb::EntitySpecifics& entity_specifics,
                      WriteNode* write_node) {
  if (GetModelTypeFromSpecifics(entity_specifics) == PASSWORDS) {
    write_node->SetPasswordSpecifics(
        entity_specifics.password().client_only_encrypted_data());
  } else {
    write_node->SetEntitySpecifics(entity_specifics);
  }
}

// Helper function to convert AttachmentId to AttachmentMetadataRecord.
sync_pb::AttachmentMetadataRecord AttachmentIdToRecord(
    const AttachmentId& attachment_id) {
  sync_pb::AttachmentMetadataRecord record;
  *record.mutable_id() = attachment_id.GetProto();
  return record;
}

// Replace |write_nodes|'s attachment ids with |attachment_ids|.
void SetAttachmentMetadata(const AttachmentIdList& attachment_ids,
                           WriteNode* write_node) {
  DCHECK(write_node);
  sync_pb::AttachmentMetadata attachment_metadata;
  std::transform(
      attachment_ids.begin(), attachment_ids.end(),
      RepeatedFieldBackInserter(attachment_metadata.mutable_record()),
      AttachmentIdToRecord);
  write_node->SetAttachmentMetadata(attachment_metadata);
}

SyncData BuildRemoteSyncData(
    int64_t sync_id,
    const ReadNode& read_node,
    const AttachmentServiceProxy& attachment_service_proxy) {
  const AttachmentIdList& attachment_ids = read_node.GetAttachmentIds();
  switch (read_node.GetModelType()) {
    case PASSWORDS: {
      // Passwords must be accessed differently, to account for their
      // encryption, and stored into a temporary EntitySpecifics.
      sync_pb::EntitySpecifics password_holder;
      password_holder.mutable_password()
          ->mutable_client_only_encrypted_data()
          ->CopyFrom(read_node.GetPasswordSpecifics());
      return SyncData::CreateRemoteData(
          sync_id, password_holder, read_node.GetModificationTime(),
          attachment_ids, attachment_service_proxy);
    }
    case SESSIONS:
      // Include tag hashes for sessions data type to allow discarding during
      // merge if re-hashing by the service gives a different value. This is to
      // allow removal of incorrectly hashed values, see crbug.com/604657. This
      // cannot be done in the processor because only the service knows how to
      // generate a tag from the specifics. We don't set this value for other
      // data types because they shouldn't need it and it costs memory to hold
      // another copy of this string around.
      return SyncData::CreateRemoteData(
          sync_id, read_node.GetEntitySpecifics(),
          read_node.GetModificationTime(), attachment_ids,
          attachment_service_proxy, read_node.GetEntry()->GetUniqueClientTag());
    default:
      // Use the specifics directly, encryption has already been handled.
      return SyncData::CreateRemoteData(sync_id, read_node.GetEntitySpecifics(),
                                        read_node.GetModificationTime(),
                                        attachment_ids,
                                        attachment_service_proxy);
  }
}

}  // namespace

GenericChangeProcessor::GenericChangeProcessor(
    ModelType type,
    std::unique_ptr<DataTypeErrorHandler> error_handler,
    const base::WeakPtr<SyncableService>& local_service,
    const base::WeakPtr<SyncMergeResult>& merge_result,
    UserShare* user_share,
    SyncClient* sync_client,
    std::unique_ptr<AttachmentStoreForSync> attachment_store)
    : ChangeProcessor(std::move(error_handler)),
      type_(type),
      local_service_(local_service),
      merge_result_(merge_result),
      share_handle_(user_share),
      weak_ptr_factory_(this) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK_NE(type_, UNSPECIFIED);
  if (attachment_store) {
    std::string store_birthday;
    {
      ReadTransaction trans(FROM_HERE, share_handle());
      store_birthday = trans.GetStoreBirthday();
    }
    attachment_service_ =
        sync_client->GetSyncApiComponentFactory()->CreateAttachmentService(
            std::move(attachment_store), *user_share, store_birthday, type,
            this);
    attachment_service_weak_ptr_factory_ =
        base::MakeUnique<base::WeakPtrFactory<AttachmentService>>(
            attachment_service_.get());
    attachment_service_proxy_ = AttachmentServiceProxy(
        base::SequencedTaskRunnerHandle::Get(),
        attachment_service_weak_ptr_factory_->GetWeakPtr());
    UploadAllAttachmentsNotOnServer();
  } else {
    attachment_service_proxy_ =
        AttachmentServiceProxy(base::SequencedTaskRunnerHandle::Get(),
                               base::WeakPtr<AttachmentService>());
  }
}

GenericChangeProcessor::~GenericChangeProcessor() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
}

void GenericChangeProcessor::ApplyChangesFromSyncModel(
    const BaseTransaction* trans,
    int64_t model_version,
    const ImmutableChangeRecordList& changes) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(syncer_changes_.empty());
  for (ChangeRecordList::const_iterator it = changes.Get().begin();
       it != changes.Get().end(); ++it) {
    if (it->action == ChangeRecord::ACTION_DELETE) {
      std::unique_ptr<sync_pb::EntitySpecifics> specifics;
      if (it->specifics.has_password()) {
        DCHECK(it->extra.get());
        specifics = base::MakeUnique<sync_pb::EntitySpecifics>(it->specifics);
        specifics->mutable_password()
            ->mutable_client_only_encrypted_data()
            ->CopyFrom(it->extra->unencrypted());
      }
      const AttachmentIdList empty_list_of_attachment_ids;
      syncer_changes_.push_back(SyncChange(
          FROM_HERE, SyncChange::ACTION_DELETE,
          SyncData::CreateRemoteData(
              it->id, specifics ? *specifics : it->specifics, base::Time(),
              empty_list_of_attachment_ids, attachment_service_proxy_)));
    } else {
      SyncChange::SyncChangeType action =
          (it->action == ChangeRecord::ACTION_ADD) ? SyncChange::ACTION_ADD
                                                   : SyncChange::ACTION_UPDATE;
      // Need to load specifics from node.
      ReadNode read_node(trans);
      if (read_node.InitByIdLookup(it->id) != BaseNode::INIT_OK) {
        SyncError error(FROM_HERE, SyncError::DATATYPE_ERROR,
                        "Failed to look up data for received change with id " +
                            base::Int64ToString(it->id),
                        GetModelTypeFromSpecifics(it->specifics));
        error_handler()->OnUnrecoverableError(error);
        return;
      }
      syncer_changes_.push_back(SyncChange(
          FROM_HERE, action,
          BuildRemoteSyncData(it->id, read_node, attachment_service_proxy_)));
    }
  }
}

void GenericChangeProcessor::CommitChangesFromSyncModel() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (syncer_changes_.empty())
    return;
  if (!local_service_.get()) {
    ModelType type = syncer_changes_[0].sync_data().GetDataType();
    SyncError error(FROM_HERE, SyncError::DATATYPE_ERROR,
                    "Local service destroyed.", type);
    error_handler()->OnUnrecoverableError(error);
    return;
  }
  SyncError error =
      local_service_->ProcessSyncChanges(FROM_HERE, syncer_changes_);
  syncer_changes_.clear();
  if (error.IsSet())
    error_handler()->OnUnrecoverableError(error);
}

SyncDataList GenericChangeProcessor::GetAllSyncData(ModelType type) const {
  DCHECK_EQ(type_, type);
  // This is slow / memory intensive.  Should be used sparingly by datatypes.
  SyncDataList data;
  GetAllSyncDataReturnError(&data);
  return data;
}

SyncError GenericChangeProcessor::UpdateDataTypeContext(
    ModelType type,
    SyncChangeProcessor::ContextRefreshStatus refresh_status,
    const std::string& context) {
  DCHECK(ProtocolTypes().Has(type));
  DCHECK_EQ(type_, type);

  if (context.size() > static_cast<size_t>(kContextSizeLimit)) {
    return SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                     "Context size limit exceeded.", type);
  }

  WriteTransaction trans(FROM_HERE, share_handle());
  trans.SetDataTypeContext(type, refresh_status, context);

  // TODO(zea): plumb a pointer to the PSS or SyncManagerImpl here so we can
  // trigger a datatype nudge if |refresh_status == REFRESH_NEEDED|.

  return SyncError();
}

void GenericChangeProcessor::AddLocalChangeObserver(
    LocalChangeObserver* observer) {
  local_change_observers_.AddObserver(observer);
}

void GenericChangeProcessor::RemoveLocalChangeObserver(
    LocalChangeObserver* observer) {
  local_change_observers_.RemoveObserver(observer);
}

void GenericChangeProcessor::OnAttachmentUploaded(
    const AttachmentId& attachment_id) {
  WriteTransaction trans(FROM_HERE, share_handle());
  trans.UpdateEntriesMarkAttachmentAsOnServer(attachment_id);
}

SyncError GenericChangeProcessor::GetAllSyncDataReturnError(
    SyncDataList* current_sync_data) const {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  std::string type_name = ModelTypeToString(type_);
  ReadTransaction trans(FROM_HERE, share_handle());
  ReadNode root(&trans);
  if (root.InitTypeRoot(type_) != BaseNode::INIT_OK) {
    SyncError error(FROM_HERE, SyncError::DATATYPE_ERROR,
                    "Server did not create the top-level " + type_name +
                        " node. We might be running against an out-of-"
                        "date server.",
                    type_);
    return error;
  }

  // TODO(akalin): We'll have to do a tree traversal for bookmarks.
  DCHECK_NE(type_, BOOKMARKS);

  std::vector<int64_t> child_ids;
  root.GetChildIds(&child_ids);

  for (std::vector<int64_t>::iterator it = child_ids.begin();
       it != child_ids.end(); ++it) {
    ReadNode sync_child_node(&trans);
    if (sync_child_node.InitByIdLookup(*it) != BaseNode::INIT_OK) {
      SyncError error(FROM_HERE, SyncError::DATATYPE_ERROR,
                      "Failed to fetch child node for type " + type_name + ".",
                      type_);
      return error;
    }
    current_sync_data->push_back(BuildRemoteSyncData(
        sync_child_node.GetId(), sync_child_node, attachment_service_proxy_));
  }
  return SyncError();
}

bool GenericChangeProcessor::GetDataTypeContext(std::string* context) const {
  ReadTransaction trans(FROM_HERE, share_handle());
  sync_pb::DataTypeContext context_proto;
  trans.GetDataTypeContext(type_, &context_proto);
  if (!context_proto.has_context())
    return false;

  DCHECK_EQ(type_,
            GetModelTypeFromSpecificsFieldNumber(context_proto.data_type_id()));
  *context = context_proto.context();
  return true;
}

int GenericChangeProcessor::GetSyncCount() {
  ReadTransaction trans(FROM_HERE, share_handle());
  ReadNode root(&trans);
  if (root.InitTypeRoot(type_) != BaseNode::INIT_OK)
    return 0;

  // Subtract one to account for type's root node.
  return root.GetTotalNodeCount() - 1;
}

namespace {

// WARNING: this code is sensitive to compiler optimizations. Be careful
// modifying any code around an OnUnrecoverableError call, else the compiler
// attempts to merge it with other calls, losing useful information in
// breakpad uploads.
SyncError LogLookupFailure(BaseNode::InitByLookupResult lookup_result,
                           const tracked_objects::Location& from_here,
                           const std::string& error_prefix,
                           ModelType type,
                           DataTypeErrorHandler* error_handler) {
  switch (lookup_result) {
    case BaseNode::INIT_FAILED_ENTRY_NOT_GOOD: {
      SyncError error;
      error.Reset(
          from_here,
          error_prefix + "could not find entry matching the lookup criteria.",
          type);
      error_handler->OnUnrecoverableError(error);
      LOG(ERROR) << "Delete: Bad entry.";
      return error;
    }
    case BaseNode::INIT_FAILED_ENTRY_IS_DEL: {
      SyncError error;
      error.Reset(from_here, error_prefix + "entry is already deleted.", type);
      error_handler->OnUnrecoverableError(error);
      LOG(ERROR) << "Delete: Deleted entry.";
      return error;
    }
    case BaseNode::INIT_FAILED_DECRYPT_IF_NECESSARY: {
      SyncError error;
      error.Reset(from_here, error_prefix + "unable to decrypt", type);
      error_handler->OnUnrecoverableError(error);
      LOG(ERROR) << "Delete: Undecryptable entry.";
      return error;
    }
    case BaseNode::INIT_FAILED_PRECONDITION: {
      SyncError error;
      error.Reset(from_here,
                  error_prefix + "a precondition was not met for calling init.",
                  type);
      error_handler->OnUnrecoverableError(error);
      LOG(ERROR) << "Delete: Failed precondition.";
      return error;
    }
    default: {
      SyncError error;
      // Should have listed all the possible error cases above.
      error.Reset(from_here, error_prefix + "unknown error", type);
      error_handler->OnUnrecoverableError(error);
      LOG(ERROR) << "Delete: Unknown error.";
      return error;
    }
  }
}

}  // namespace

SyncError GenericChangeProcessor::AttemptDelete(
    const SyncChange& change,
    ModelType type,
    const std::string& type_str,
    WriteNode* node,
    DataTypeErrorHandler* error_handler) {
  DCHECK_EQ(change.change_type(), SyncChange::ACTION_DELETE);
  if (change.sync_data().IsLocal()) {
    const std::string& tag = SyncDataLocal(change.sync_data()).GetTag();
    if (tag.empty()) {
      SyncError error(FROM_HERE, SyncError::DATATYPE_ERROR,
                      "Failed to delete " + type_str +
                          " node. Local data, empty tag. " +
                          change.location().ToString(),
                      type);
      error_handler->OnUnrecoverableError(error);
      NOTREACHED();
      return error;
    }

    BaseNode::InitByLookupResult result =
        node->InitByClientTagLookup(change.sync_data().GetDataType(), tag);
    if (result != BaseNode::INIT_OK) {
      return LogLookupFailure(result, FROM_HERE,
                              "Failed to delete " + type_str +
                                  " node. Local data. " +
                                  change.location().ToString(),
                              type, error_handler);
    }
  } else {
    BaseNode::InitByLookupResult result =
        node->InitByIdLookup(SyncDataRemote(change.sync_data()).GetId());
    if (result != BaseNode::INIT_OK) {
      return LogLookupFailure(result, FROM_HERE,
                              "Failed to delete " + type_str +
                                  " node. Non-local data. " +
                                  change.location().ToString(),
                              type, error_handler);
    }
  }
  NotifyLocalChangeObservers(node->GetEntry(), change);
  if (IsActOnceDataType(type))
    node->Drop();
  else
    node->Tombstone();
  return SyncError();
}

SyncError GenericChangeProcessor::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& list_of_changes) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  if (list_of_changes.empty()) {
    // No work. Exit without entering WriteTransaction.
    return SyncError();
  }

  // Keep track of brand new attachments so we can persist them on this device
  // and upload them to the server.
  AttachmentIdSet new_attachments;

  WriteTransaction trans(from_here, share_handle());

  for (SyncChangeList::const_iterator iter = list_of_changes.begin();
       iter != list_of_changes.end(); ++iter) {
    const SyncChange& change = *iter;
    DCHECK_EQ(change.sync_data().GetDataType(), type_);
    std::string type_str = ModelTypeToString(type_);
    WriteNode sync_node(&trans);
    if (change.change_type() == SyncChange::ACTION_DELETE) {
      SyncError error =
          AttemptDelete(change, type_, type_str, &sync_node, error_handler());
      if (error.IsSet()) {
        NOTREACHED();
        return error;
      }
      if (merge_result_.get()) {
        merge_result_->set_num_items_deleted(
            merge_result_->num_items_deleted() + 1);
      }
    } else if (change.change_type() == SyncChange::ACTION_ADD) {
      SyncError error = HandleActionAdd(change, type_str, trans, &sync_node,
                                        &new_attachments);
      if (error.IsSet()) {
        return error;
      }
    } else if (change.change_type() == SyncChange::ACTION_UPDATE) {
      SyncError error = HandleActionUpdate(change, type_str, trans, &sync_node,
                                           &new_attachments);
      if (error.IsSet()) {
        return error;
      }
    } else {
      SyncError error(FROM_HERE, SyncError::DATATYPE_ERROR,
                      "Received unset SyncChange in the change processor, " +
                          change.location().ToString(),
                      type_);
      error_handler()->OnUnrecoverableError(error);
      NOTREACHED();
      LOG(ERROR) << "Unset sync change.";
      return error;
    }
  }

  if (!new_attachments.empty()) {
    // If datatype uses attachments it should have supplied attachment store
    // which would initialize attachment_service_. Fail if it isn't so.
    if (!attachment_service_.get()) {
      SyncError error(
          FROM_HERE, SyncError::DATATYPE_ERROR,
          "Datatype performs attachment operation without initializing "
          "attachment store",
          type_);
      error_handler()->OnUnrecoverableError(error);
      NOTREACHED();
      return error;
    }
    AttachmentIdList ids_to_upload;
    ids_to_upload.reserve(new_attachments.size());
    std::copy(new_attachments.begin(), new_attachments.end(),
              std::back_inserter(ids_to_upload));
    attachment_service_->UploadAttachments(ids_to_upload);
  }

  return SyncError();
}

// WARNING: this code is sensitive to compiler optimizations. Be careful
// modifying any code around an OnUnrecoverableError call, else the compiler
// attempts to merge it with other calls, losing useful information in
// breakpad uploads.
SyncError GenericChangeProcessor::HandleActionAdd(
    const SyncChange& change,
    const std::string& type_str,
    const WriteTransaction& trans,
    WriteNode* sync_node,
    AttachmentIdSet* new_attachments) {
  // TODO(sync): Handle other types of creation (custom parents, folders,
  // etc.).
  const SyncDataLocal sync_data_local(change.sync_data());
  WriteNode::InitUniqueByCreationResult result =
      sync_node->InitUniqueByCreation(sync_data_local.GetDataType(),
                                      sync_data_local.GetTag());
  if (result != WriteNode::INIT_SUCCESS) {
    std::string error_prefix = "Failed to create " + type_str + " node: " +
                               change.location().ToString() + ", ";
    switch (result) {
      case WriteNode::INIT_FAILED_EMPTY_TAG: {
        SyncError error;
        error.Reset(FROM_HERE, error_prefix + "empty tag", type_);
        error_handler()->OnUnrecoverableError(error);
        LOG(ERROR) << "Create: Empty tag.";
        return error;
      }
      case WriteNode::INIT_FAILED_COULD_NOT_CREATE_ENTRY: {
        SyncError error;
        error.Reset(FROM_HERE, error_prefix + "failed to create entry", type_);
        error_handler()->OnUnrecoverableError(error);
        LOG(ERROR) << "Create: Could not create entry.";
        return error;
      }
      case WriteNode::INIT_FAILED_SET_PREDECESSOR: {
        SyncError error;
        error.Reset(FROM_HERE, error_prefix + "failed to set predecessor",
                    type_);
        error_handler()->OnUnrecoverableError(error);
        LOG(ERROR) << "Create: Bad predecessor.";
        return error;
      }
      case WriteNode::INIT_FAILED_DECRYPT_EXISTING_ENTRY: {
        SyncError error;
        error.Reset(FROM_HERE, error_prefix + "failed to decrypt", type_);
        error_handler()->OnUnrecoverableError(error);
        LOG(ERROR) << "Create: Failed to decrypt.";
        return error;
      }
      default: {
        SyncError error;
        error.Reset(FROM_HERE, error_prefix + "unknown error", type_);
        error_handler()->OnUnrecoverableError(error);
        LOG(ERROR) << "Create: Unknown error.";
        return error;
      }
    }
  }
  NotifyLocalChangeObservers(sync_node->GetEntry(), change);

  sync_node->SetTitle(change.sync_data().GetTitle());
  SetNodeSpecifics(sync_data_local.GetSpecifics(), sync_node);

  AttachmentIdList attachment_ids = sync_data_local.GetAttachmentIds();
  SetAttachmentMetadata(attachment_ids, sync_node);

  // Return any newly added attachments.
  new_attachments->insert(attachment_ids.begin(), attachment_ids.end());
  if (merge_result_.get()) {
    merge_result_->set_num_items_added(merge_result_->num_items_added() + 1);
  }
  return SyncError();
}
// WARNING: this code is sensitive to compiler optimizations. Be careful
// modifying any code around an OnUnrecoverableError call, else the compiler
// attempts to merge it with other calls, losing useful information in
// breakpad uploads.
SyncError GenericChangeProcessor::HandleActionUpdate(
    const SyncChange& change,
    const std::string& type_str,
    const WriteTransaction& trans,
    WriteNode* sync_node,
    AttachmentIdSet* new_attachments) {
  const SyncDataLocal sync_data_local(change.sync_data());
  BaseNode::InitByLookupResult result = sync_node->InitByClientTagLookup(
      sync_data_local.GetDataType(), sync_data_local.GetTag());
  if (result != BaseNode::INIT_OK) {
    std::string error_prefix = "Failed to load " + type_str + " node. " +
                               change.location().ToString() + ", ";
    if (result == BaseNode::INIT_FAILED_PRECONDITION) {
      SyncError error;
      error.Reset(FROM_HERE, error_prefix + "empty tag", type_);
      error_handler()->OnUnrecoverableError(error);
      LOG(ERROR) << "Update: Empty tag.";
      return error;
    } else if (result == BaseNode::INIT_FAILED_ENTRY_NOT_GOOD) {
      SyncError error;
      error.Reset(FROM_HERE, error_prefix + "bad entry", type_);
      error_handler()->OnUnrecoverableError(error);
      LOG(ERROR) << "Update: bad entry.";
      return error;
    } else if (result == BaseNode::INIT_FAILED_ENTRY_IS_DEL) {
      SyncError error;
      error.Reset(FROM_HERE, error_prefix + "deleted entry", type_);
      error_handler()->OnUnrecoverableError(error);
      LOG(ERROR) << "Update: deleted entry.";
      return error;
    } else if (result == BaseNode::INIT_FAILED_DECRYPT_IF_NECESSARY) {
      SyncError error;
      error.Reset(FROM_HERE, error_prefix + "failed to decrypt", type_);
      error_handler()->OnUnrecoverableError(error);
      LOG(ERROR) << "Update: Failed to decrypt.";
      return error;
    } else {
      NOTREACHED();
      SyncError error;
      error.Reset(FROM_HERE, error_prefix + "unknown error", type_);
      error_handler()->OnUnrecoverableError(error);
      LOG(ERROR) << "Update: Unknown error.";
      return error;
    }
  }

  NotifyLocalChangeObservers(sync_node->GetEntry(), change);

  sync_node->SetTitle(change.sync_data().GetTitle());
  SetNodeSpecifics(sync_data_local.GetSpecifics(), sync_node);
  AttachmentIdList attachment_ids = sync_data_local.GetAttachmentIds();
  SetAttachmentMetadata(attachment_ids, sync_node);

  // Return any newly added attachments.
  new_attachments->insert(attachment_ids.begin(), attachment_ids.end());

  if (merge_result_.get()) {
    merge_result_->set_num_items_modified(merge_result_->num_items_modified() +
                                          1);
  }
  // TODO(sync): Support updating other parts of the sync node (title,
  // successor, parent, etc.).
  return SyncError();
}

bool GenericChangeProcessor::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(has_nodes);
  std::string type_name = ModelTypeToString(type_);
  std::string err_str =
      "Server did not create the top-level " + type_name +
      " node. We might be running against an out-of-date server.";
  *has_nodes = false;
  ReadTransaction trans(FROM_HERE, share_handle());
  ReadNode type_root_node(&trans);
  if (type_root_node.InitTypeRoot(type_) != BaseNode::INIT_OK) {
    LOG(ERROR) << err_str;
    return false;
  }

  // The sync model has user created nodes if the type's root node has any
  // children.
  *has_nodes = type_root_node.HasChildren();
  return true;
}

bool GenericChangeProcessor::CryptoReadyIfNecessary() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  // We only access the cryptographer while holding a transaction.
  ReadTransaction trans(FROM_HERE, share_handle());
  const ModelTypeSet encrypted_types = trans.GetEncryptedTypes();
  return !encrypted_types.Has(type_) || trans.GetCryptographer()->is_ready();
}

void GenericChangeProcessor::StartImpl() {}

UserShare* GenericChangeProcessor::share_handle() const {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return share_handle_;
}

void GenericChangeProcessor::UploadAllAttachmentsNotOnServer() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(attachment_service_.get());
  AttachmentIdList ids;
  {
    ReadTransaction trans(FROM_HERE, share_handle());
    trans.GetAttachmentIdsToUpload(type_, &ids);
  }
  if (!ids.empty()) {
    attachment_service_->UploadAttachments(ids);
  }
}

void GenericChangeProcessor::NotifyLocalChangeObservers(
    const syncable::Entry* current_entry,
    const SyncChange& change) {
  for (auto& observer : local_change_observers_)
    observer.OnLocalChange(current_entry, change);
}

std::unique_ptr<AttachmentService>
GenericChangeProcessor::GetAttachmentService() const {
  return std::unique_ptr<AttachmentService>(
      new AttachmentServiceProxy(attachment_service_proxy_));
}

}  // namespace syncer
