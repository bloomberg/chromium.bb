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

 private:
  std::multimap<std::string, std::unique_ptr<EntityData>> put_multimap_;
  std::set<std::string> delete_set_;
  std::unique_ptr<MetadataBatch> metadata_;
  bool is_tracking_metadata_ = true;
};

}  //  namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_RECORDING_MODEL_TYPE_CHANGE_PROCESSOR_H_
