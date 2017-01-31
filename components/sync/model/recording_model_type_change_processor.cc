// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/recording_model_type_change_processor.h"

#include <utility>

#include "components/sync/model/metadata_batch.h"

namespace syncer {

RecordingModelTypeChangeProcessor::RecordingModelTypeChangeProcessor() {}

RecordingModelTypeChangeProcessor::~RecordingModelTypeChangeProcessor() {}

void RecordingModelTypeChangeProcessor::Put(
    const std::string& storage_key,
    std::unique_ptr<EntityData> entity_data,
    MetadataChangeList* metadata_changes) {
  put_multimap_.insert(std::make_pair(storage_key, std::move(entity_data)));
}

void RecordingModelTypeChangeProcessor::Delete(
    const std::string& storage_key,
    MetadataChangeList* metadata_changes) {
  delete_set_.insert(storage_key);
}

void RecordingModelTypeChangeProcessor::ModelReadyToSync(
    std::unique_ptr<MetadataBatch> batch) {
  std::swap(metadata_, batch);
}

bool RecordingModelTypeChangeProcessor::IsTrackingMetadata() {
  return is_tracking_metadata_;
}

void RecordingModelTypeChangeProcessor::SetIsTrackingMetadata(
    bool is_tracking) {
  is_tracking_metadata_ = is_tracking;
}

}  //  namespace syncer
