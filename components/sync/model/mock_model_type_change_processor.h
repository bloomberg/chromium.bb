// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MOCK_MODEL_TYPE_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_MODEL_MOCK_MODEL_TYPE_CHANGE_PROCESSOR_H_

#include <memory>
#include <string>

#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class MockModelTypeChangeProcessor : public ModelTypeChangeProcessor {
 public:
  MockModelTypeChangeProcessor();
  virtual ~MockModelTypeChangeProcessor();

  // TODO(crbug.com/729950): Use unique_ptr here direclty once move-only
  // arguments are supported in gMock.
  MOCK_METHOD3(DoPut,
               void(const std::string& storage_key,
                    EntityData* entity_data,
                    MetadataChangeList* metadata_change_list));
  void Put(const std::string& storage_key,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_change_list) override;
  MOCK_METHOD2(Delete,
               void(const std::string& storage_key,
                    MetadataChangeList* metadata_change_list));
  MOCK_METHOD3(UpdateStorageKey,
               void(const EntityData& entity_data,
                    const std::string& storage_key,
                    MetadataChangeList* metadata_change_list));
  MOCK_METHOD1(UntrackEntity, void(const EntityData& entity_data));
  // TODO(crbug.com/729950): Use unique_ptr here directly once move-only
  // arguments are supported in gMock.
  MOCK_METHOD2(DoModelReadyToSync,
               void(ModelTypeSyncBridge* bridge, MetadataBatch* batch));
  void ModelReadyToSync(ModelTypeSyncBridge* bridge,
                        std::unique_ptr<MetadataBatch> batch) override;
  MOCK_METHOD2(OnSyncStarting,
               void(const ModelErrorHandler& error_handler,
                    const StartCallback& callback));
  MOCK_METHOD0(DisableSync, void());
  MOCK_METHOD0(IsTrackingMetadata, bool());
  MOCK_METHOD1(ReportError, void(const ModelError& error));

  // Returns a processor that forwards all calls to
  // |this|. |*this| must outlive the returned processor.
  std::unique_ptr<ModelTypeChangeProcessor> CreateForwardingProcessor();
};

}  //  namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MOCK_MODEL_TYPE_CHANGE_PROCESSOR_H_
