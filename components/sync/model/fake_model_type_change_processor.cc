// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/fake_model_type_change_processor.h"

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

// static
std::unique_ptr<ModelTypeChangeProcessor> FakeModelTypeChangeProcessor::Create(
    ModelType type,
    ModelTypeSyncBridge* bridge) {
  return base::WrapUnique(new FakeModelTypeChangeProcessor());
}

FakeModelTypeChangeProcessor::FakeModelTypeChangeProcessor() = default;

FakeModelTypeChangeProcessor::~FakeModelTypeChangeProcessor() {
  // If this fails we were expecting an error but never got one.
  EXPECT_FALSE(expect_error_);
}

void FakeModelTypeChangeProcessor::Put(
    const std::string& client_tag,
    std::unique_ptr<EntityData> entity_data,
    MetadataChangeList* metadata_change_list) {}

void FakeModelTypeChangeProcessor::Delete(
    const std::string& client_tag,
    MetadataChangeList* metadata_change_list) {}

void FakeModelTypeChangeProcessor::UpdateStorageKey(
    const EntityData& entity_data,
    const std::string& storage_key,
    MetadataChangeList* metadata_change_list) {}

void FakeModelTypeChangeProcessor::UntrackEntity(
    const EntityData& entity_data) {}

void FakeModelTypeChangeProcessor::ModelReadyToSync(
    std::unique_ptr<MetadataBatch> batch) {}

void FakeModelTypeChangeProcessor::OnSyncStarting(
    const ModelErrorHandler& error_handler,
    const StartCallback& callback) {
  if (!callback.is_null()) {
    callback.Run(nullptr);
  }
}

void FakeModelTypeChangeProcessor::DisableSync() {}

bool FakeModelTypeChangeProcessor::IsTrackingMetadata() {
  return true;
}

void FakeModelTypeChangeProcessor::ReportError(const ModelError& error) {
  EXPECT_TRUE(expect_error_);
  expect_error_ = false;
}

void FakeModelTypeChangeProcessor::ReportError(
    const tracked_objects::Location& location,
    const std::string& message) {
  ReportError(ModelError(location, message));
}

void FakeModelTypeChangeProcessor::ExpectError() {
  expect_error_ = true;
}

}  // namespace syncer
