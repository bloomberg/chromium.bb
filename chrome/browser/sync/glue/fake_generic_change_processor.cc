// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/fake_generic_change_processor.h"

#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "sync/api/syncable_service.h"

namespace browser_sync {

FakeGenericChangeProcessor::FakeGenericChangeProcessor()
    : GenericChangeProcessor(NULL, base::WeakPtr<SyncableService>(), NULL),
      sync_model_has_user_created_nodes_(true),
      sync_model_has_user_created_nodes_success_(true),
      crypto_ready_if_necessary_(true),
      type_(syncable::UNSPECIFIED) {}

FakeGenericChangeProcessor::~FakeGenericChangeProcessor() {}

void FakeGenericChangeProcessor::set_process_sync_changes_error(
    const SyncError& error) {
  process_sync_changes_error_ = error;
}
void FakeGenericChangeProcessor::set_get_sync_data_for_type_error(
    const SyncError& error) {
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

SyncError FakeGenericChangeProcessor::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  return process_sync_changes_error_;
}

SyncError FakeGenericChangeProcessor::GetSyncDataForType(
    syncable::ModelType type, SyncDataList* current_sync_data) {
  type_ = type;
  return get_sync_data_for_type_error_;
}

bool FakeGenericChangeProcessor::SyncModelHasUserCreatedNodes(
    syncable::ModelType type, bool* has_nodes) {
  type_ = type;
  *has_nodes = sync_model_has_user_created_nodes_;
  return sync_model_has_user_created_nodes_success_;
}

bool FakeGenericChangeProcessor::CryptoReadyIfNecessary(
    syncable::ModelType type) {
  type_ = type;
  return crypto_ready_if_necessary_;
}

}  // namespace browser_sync
