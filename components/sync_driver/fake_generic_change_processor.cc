// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/fake_generic_change_processor.h"

#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "sync/api/syncable_service.h"
#include "sync/internal_api/public/attachments/attachment_service_impl.h"

namespace sync_driver {

FakeGenericChangeProcessor::FakeGenericChangeProcessor(
    syncer::ModelType type,
    SyncApiComponentFactory* sync_factory)
    : GenericChangeProcessor(type,
                             NULL,
                             base::WeakPtr<syncer::SyncableService>(),
                             base::WeakPtr<syncer::SyncMergeResult>(),
                             NULL,
                             sync_factory,
                             scoped_refptr<syncer::AttachmentStore>()),
      sync_model_has_user_created_nodes_(true),
      sync_model_has_user_created_nodes_success_(true) {
}

FakeGenericChangeProcessor::~FakeGenericChangeProcessor() {}

void FakeGenericChangeProcessor::set_sync_model_has_user_created_nodes(
    bool has_nodes) {
  sync_model_has_user_created_nodes_ = has_nodes;
}
void FakeGenericChangeProcessor::set_sync_model_has_user_created_nodes_success(
    bool success) {
  sync_model_has_user_created_nodes_success_ = success;
}

syncer::SyncError FakeGenericChangeProcessor::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  return syncer::SyncError();
}

syncer::SyncError FakeGenericChangeProcessor::GetAllSyncDataReturnError(
    syncer::SyncDataList* current_sync_data) const {
  return syncer::SyncError();
}

bool FakeGenericChangeProcessor::GetDataTypeContext(
    std::string* context) const {
  return false;
}

int FakeGenericChangeProcessor::GetSyncCount() {
  return 0;
}

bool FakeGenericChangeProcessor::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  *has_nodes = sync_model_has_user_created_nodes_;
  return sync_model_has_user_created_nodes_success_;
}

bool FakeGenericChangeProcessor::CryptoReadyIfNecessary() {
  return true;
}

FakeGenericChangeProcessorFactory::FakeGenericChangeProcessorFactory(
    scoped_ptr<FakeGenericChangeProcessor> processor)
    : processor_(processor.Pass()) {}

FakeGenericChangeProcessorFactory::~FakeGenericChangeProcessorFactory() {}

scoped_ptr<GenericChangeProcessor>
FakeGenericChangeProcessorFactory::CreateGenericChangeProcessor(
    syncer::ModelType type,
    syncer::UserShare* user_share,
    DataTypeErrorHandler* error_handler,
    const base::WeakPtr<syncer::SyncableService>& local_service,
    const base::WeakPtr<syncer::SyncMergeResult>& merge_result,
    SyncApiComponentFactory* sync_factory) {
  return processor_.PassAs<GenericChangeProcessor>();
}

}  // namespace sync_driver
