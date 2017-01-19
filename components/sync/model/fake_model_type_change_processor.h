// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_FAKE_MODEL_TYPE_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_MODEL_FAKE_MODEL_TYPE_CHANGE_PROCESSOR_H_

#include <memory>
#include <string>

#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"

namespace syncer {

class ModelTypeSyncBridge;

// A ModelTypeChangeProcessor implementation for tests.
class FakeModelTypeChangeProcessor : public ModelTypeChangeProcessor {
 public:
  static std::unique_ptr<ModelTypeChangeProcessor> Create(
      ModelType type,
      ModelTypeSyncBridge* bridge);

  FakeModelTypeChangeProcessor();
  ~FakeModelTypeChangeProcessor() override;

  // ModelTypeChangeProcessor overrides
  void Put(const std::string& client_tag,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_change_list) override;
  void Delete(const std::string& client_tag,
              MetadataChangeList* metadata_change_list) override;
  void ModelReadyToSync(std::unique_ptr<MetadataBatch> batch) override;
  void OnSyncStarting(const ModelErrorHandler& error_handler,
                      const StartCallback& callback) override;
  void DisableSync() override;
  bool IsTrackingMetadata() override;
  void ReportError(const ModelError& error) override;
  void ReportError(const tracked_objects::Location& location,
                   const std::string& message) override;

  // Indicates that ReportError should be called in the future.
  void ExpectError();

 private:
  // Whether we expect ReportError to be called.
  bool expect_error_ = false;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_FAKE_MODEL_TYPE_CHANGE_PROCESSOR_H_
