// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_RECORDING_MODEL_TYPE_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_MODEL_RECORDING_MODEL_TYPE_CHANGE_PROCESSOR_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "components/sync/model/fake_model_type_change_processor.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {

// Augmented FakeModelTypeChangeProcessor that accumulates all instructions in
// members that can then be accessed for verification.
class RecordingModelTypeChangeProcessor : public FakeModelTypeChangeProcessor {
 public:
  RecordingModelTypeChangeProcessor();
  ~RecordingModelTypeChangeProcessor() override;

  // FakeModelTypeChangeProcessor overrides.
  void Put(const std::string& storage_key,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_changes) override;
  void Delete(const std::string& storage_key,
              MetadataChangeList* metadata_changes) override;
  void ModelReadyToSync(std::unique_ptr<MetadataBatch> batch) override;
  bool IsTrackingMetadata() override;

  void SetIsTrackingMetadata(bool is_tracking);

  const std::multimap<std::string, std::unique_ptr<EntityData>>& put_multimap()
      const {
    return put_multimap_;
  }

  const std::set<std::string>& delete_set() const { return delete_set_; }

  MetadataBatch* metadata() const { return metadata_.get(); }

  // Returns a callback that constructs a processor and assigns a raw pointer to
  // the given address. The caller must ensure that the address passed in is
  // still valid whenever the callback is run. This can be useful for tests that
  // want to verify the RecordingModelTypeChangeProcessor was given data by the
  // bridge they are testing.
  static ModelTypeSyncBridge::ChangeProcessorFactory FactoryForBridgeTest(
      RecordingModelTypeChangeProcessor** processor_address,
      bool expect_error = false);

 private:
  std::multimap<std::string, std::unique_ptr<EntityData>> put_multimap_;
  std::set<std::string> delete_set_;
  std::unique_ptr<MetadataBatch> metadata_;
  bool is_tracking_metadata_ = true;
};

}  //  namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_RECORDING_MODEL_TYPE_CHANGE_PROCESSOR_H_
