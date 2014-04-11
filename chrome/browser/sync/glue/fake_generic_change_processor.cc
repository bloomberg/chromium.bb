// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/fake_generic_change_processor.h"

#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "sync/api/attachments/fake_attachment_service.h"
#include "sync/api/syncable_service.h"

namespace browser_sync {

FakeGenericChangeProcessor::FakeGenericChangeProcessor()
    : GenericChangeProcessor(NULL,
                             base::WeakPtr<syncer::SyncableService>(),
                             base::WeakPtr<syncer::SyncMergeResult>(),
                             NULL,
                             syncer::FakeAttachmentService::CreateForTest()),
      sync_model_has_user_created_nodes_(true),
      sync_model_has_user_created_nodes_success_(true),
      crypto_ready_if_necessary_(true) {}

FakeGenericChangeProcessor::~FakeGenericChangeProcessor() {}

void FakeGenericChangeProcessor::set_process_sync_changes_error(
    const syncer::SyncError& error) {
  process_sync_changes_error_ = error;
}
void FakeGenericChangeProcessor::set_get_sync_data_for_type_error(
    const syncer::SyncError& error) {
  get_sync_data_for_type_error_ = error;
}
void FakeGenericChangeProcessor::set_sync_model_has_user_created_nodes(
    bool has_nodes) {
  sync_model_has_user_created_nodes_ = has_nodes;
}
void FakeGenericChangeProcessor::set_sync_model_has_user_created_nodes_success(
    bool success) {
  sync_model_has_user_created_nodes_success_ = success;
}
void FakeGenericChangeProcessor::set_crypto_ready_if_necessary(
    bool crypto_ready) {
  crypto_ready_if_necessary_ = crypto_ready;
}

syncer::SyncError FakeGenericChangeProcessor::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  return process_sync_changes_error_;
}

syncer::SyncError FakeGenericChangeProcessor::GetAllSyncDataReturnError(
    syncer::ModelType type, syncer::SyncDataList* current_sync_data) const {
  return get_sync_data_for_type_error_;
}

bool FakeGenericChangeProcessor::GetDataTypeContext(
    syncer::ModelType type,
    std::string* context) const {
  return false;
}

int FakeGenericChangeProcessor::GetSyncCountForType(syncer::ModelType type) {
  return 0;
}

bool FakeGenericChangeProcessor::SyncModelHasUserCreatedNodes(
    syncer::ModelType type, bool* has_nodes) {
  *has_nodes = sync_model_has_user_created_nodes_;
  return sync_model_has_user_created_nodes_success_;
}

bool FakeGenericChangeProcessor::CryptoReadyIfNecessary(
    syncer::ModelType type) {
  return crypto_ready_if_necessary_;
}

}  // namespace browser_sync
