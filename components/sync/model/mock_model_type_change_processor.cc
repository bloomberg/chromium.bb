// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/mock_model_type_change_processor.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/sync/model/metadata_batch.h"

namespace syncer {
namespace {

class ForwardingModelTypeChangeProcessor : public ModelTypeChangeProcessor {
 public:
  // |other| must not be nullptr and must outlive this object.
  explicit ForwardingModelTypeChangeProcessor(ModelTypeChangeProcessor* other)
      : other_(other) {}
  ~ForwardingModelTypeChangeProcessor() override{};

  void Put(const std::string& client_tag,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_change_list) override {
    other_->Put(client_tag, std::move(entity_data), metadata_change_list);
  }

  void Delete(const std::string& client_tag,
              MetadataChangeList* metadata_change_list) override {
    other_->Delete(client_tag, metadata_change_list);
  }

  void UpdateStorageKey(const EntityData& entity_data,
                        const std::string& storage_key,
                        MetadataChangeList* metadata_change_list) override {
    other_->UpdateStorageKey(entity_data, storage_key, metadata_change_list);
  }

  void UntrackEntity(const EntityData& entity_data) override {
    other_->UntrackEntity(entity_data);
  }

  void ModelReadyToSync(ModelTypeSyncBridge* bridge,
                        std::unique_ptr<MetadataBatch> batch) override {
    other_->ModelReadyToSync(bridge, std::move(batch));
  }

  void OnSyncStarting(const ModelErrorHandler& error_handler,
                      const StartCallback& callback) override {
    other_->OnSyncStarting(error_handler, callback);
  }

  void DisableSync() override { other_->DisableSync(); }

  bool IsTrackingMetadata() override { return other_->IsTrackingMetadata(); }

  void ReportError(const ModelError& error) override {
    other_->ReportError(error);
  }

 private:
  ModelTypeChangeProcessor* other_;
};

}  // namespace

MockModelTypeChangeProcessor::MockModelTypeChangeProcessor() {}

MockModelTypeChangeProcessor::~MockModelTypeChangeProcessor() {}

void MockModelTypeChangeProcessor::Put(
    const std::string& storage_key,
    std::unique_ptr<EntityData> entity_data,
    MetadataChangeList* metadata_change_list) {
  DoPut(storage_key, entity_data.get(), metadata_change_list);
}

void MockModelTypeChangeProcessor::ModelReadyToSync(
    ModelTypeSyncBridge* bridge,
    std::unique_ptr<MetadataBatch> batch) {
  DoModelReadyToSync(bridge, batch.get());
}

std::unique_ptr<ModelTypeChangeProcessor>
MockModelTypeChangeProcessor::CreateForwardingProcessor() {
  return base::WrapUnique<ModelTypeChangeProcessor>(
      new ForwardingModelTypeChangeProcessor(this));
}

}  //  namespace syncer
