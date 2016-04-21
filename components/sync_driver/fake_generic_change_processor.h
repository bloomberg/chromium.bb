// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_FAKE_GENERIC_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_DRIVER_FAKE_GENERIC_CHANGE_PROCESSOR_H_

#include <string>

#include "base/macros.h"
#include "components/sync_driver/generic_change_processor.h"
#include "components/sync_driver/generic_change_processor_factory.h"
#include "components/sync_driver/sync_api_component_factory.h"
#include "sync/api/sync_error.h"
#include "sync/internal_api/public/base/model_type.h"

namespace sync_driver {

// A fake GenericChangeProcessor that can return arbitrary values.
class FakeGenericChangeProcessor : public GenericChangeProcessor {
 public:
  FakeGenericChangeProcessor(syncer::ModelType type,
                             SyncClient* sync_client);
  ~FakeGenericChangeProcessor() override;

  // Setters for GenericChangeProcessor implementation results.
  void set_sync_model_has_user_created_nodes(bool has_nodes);
  void set_sync_model_has_user_created_nodes_success(bool success);

  // GenericChangeProcessor implementations.
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) override;
  syncer::SyncError GetAllSyncDataReturnError(
      syncer::SyncDataList* data) const override;
  bool GetDataTypeContext(std::string* context) const override;
  int GetSyncCount() override;
  bool SyncModelHasUserCreatedNodes(bool* has_nodes) override;
  bool CryptoReadyIfNecessary() override;

 private:
  bool sync_model_has_user_created_nodes_;
  bool sync_model_has_user_created_nodes_success_;
};

// Define a factory for FakeGenericChangeProcessor for convenience.
class FakeGenericChangeProcessorFactory : public GenericChangeProcessorFactory {
 public:
  explicit FakeGenericChangeProcessorFactory(
      std::unique_ptr<FakeGenericChangeProcessor> processor);
  ~FakeGenericChangeProcessorFactory() override;
  std::unique_ptr<GenericChangeProcessor> CreateGenericChangeProcessor(
      syncer::ModelType type,
      syncer::UserShare* user_share,
      DataTypeErrorHandler* error_handler,
      const base::WeakPtr<syncer::SyncableService>& local_service,
      const base::WeakPtr<syncer::SyncMergeResult>& merge_result,
      SyncClient* sync_client) override;

 private:
  std::unique_ptr<FakeGenericChangeProcessor> processor_;
  DISALLOW_COPY_AND_ASSIGN(FakeGenericChangeProcessorFactory);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_FAKE_GENERIC_CHANGE_PROCESSOR_H_
