// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_sync_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/sync/model/fake_model_type_change_processor.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/stub_model_type_sync_bridge.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

// A mock MTCP that lets verify DisableSync and ModelReadyToSync were called in
// the ways that we expect.
class MockModelTypeChangeProcessor : public FakeModelTypeChangeProcessor {
 public:
  explicit MockModelTypeChangeProcessor(const base::Closure& disabled_callback)
      : disabled_callback_(disabled_callback) {}
  ~MockModelTypeChangeProcessor() override {}

  void DisableSync() override { disabled_callback_.Run(); }

  void ModelReadyToSync(std::unique_ptr<MetadataBatch> batch) override {
    metadata_batch_ = std::move(batch);
  }

  MetadataBatch* metadata_batch() { return metadata_batch_.get(); }

 private:
  // This callback is invoked when DisableSync() is called, instead of
  // remembering that this event happened in our own state. The reason for this
  // is that after DisableSync() is called on us, the bridge is going to
  // destroy this processor instance, and any state would be lost. The callback
  // allows this information to reach somewhere safe instead.
  base::Closure disabled_callback_;

  std::unique_ptr<MetadataBatch> metadata_batch_;
};

class MockModelTypeSyncBridge : public StubModelTypeSyncBridge {
 public:
  MockModelTypeSyncBridge()
      : StubModelTypeSyncBridge(
            base::Bind(&MockModelTypeSyncBridge::CreateProcessor,
                       base::Unretained(this))) {}
  ~MockModelTypeSyncBridge() override {}

  MockModelTypeChangeProcessor* change_processor() const {
    return static_cast<MockModelTypeChangeProcessor*>(
        ModelTypeSyncBridge::change_processor());
  }

  bool processor_disable_sync_called() const {
    return processor_disable_sync_called_;
  }

 private:
  std::unique_ptr<ModelTypeChangeProcessor> CreateProcessor(
      ModelType type,
      ModelTypeSyncBridge* bridge) {
    return base::MakeUnique<MockModelTypeChangeProcessor>(
        base::Bind(&MockModelTypeSyncBridge::OnProcessorDisableSync,
                   base::Unretained(this)));
  }

  void OnProcessorDisableSync() { processor_disable_sync_called_ = true; }

  bool processor_disable_sync_called_ = false;
};

class ModelTypeSyncBridgeTest : public ::testing::Test {
 public:
  ModelTypeSyncBridgeTest() {}
  ~ModelTypeSyncBridgeTest() override {}

  void OnSyncStarting() {
    bridge_.OnSyncStarting(
        ModelErrorHandler(),
        base::Bind(&ModelTypeSyncBridgeTest::OnProcessorStarted,
                   base::Unretained(this)));
  }

  bool start_callback_called() const { return start_callback_called_; }
  MockModelTypeSyncBridge* bridge() { return &bridge_; }

 private:
  void OnProcessorStarted(
      std::unique_ptr<ActivationContext> activation_context) {
    start_callback_called_ = true;
  }

  bool start_callback_called_ = false;
  MockModelTypeSyncBridge bridge_;
};

// OnSyncStarting should create a processor and call OnSyncStarting on it.
TEST_F(ModelTypeSyncBridgeTest, OnSyncStarting) {
  EXPECT_FALSE(start_callback_called());
  OnSyncStarting();

  // FakeModelTypeProcessor is the one that calls the callback, so if it was
  // called then we know the call on the processor was made.
  EXPECT_TRUE(start_callback_called());
}

// DisableSync should call DisableSync on the processor and then delete it.
TEST_F(ModelTypeSyncBridgeTest, DisableSync) {
  EXPECT_FALSE(bridge()->processor_disable_sync_called());
  bridge()->DisableSync();

  // Disabling also wipes out metadata, and the bridge should have told the new
  // processor about this.
  EXPECT_TRUE(bridge()->processor_disable_sync_called());

  MetadataBatch* batch = bridge()->change_processor()->metadata_batch();
  EXPECT_NE(nullptr, batch);
  EXPECT_EQ(sync_pb::ModelTypeState().SerializeAsString(),
            batch->GetModelTypeState().SerializeAsString());
  EXPECT_EQ(0U, batch->TakeAllMetadata().size());
}

// ResolveConflicts should return USE_REMOTE unless the remote data is deleted.
TEST_F(ModelTypeSyncBridgeTest, DefaultConflictResolution) {
  EntityData local_data;
  EntityData remote_data;

  // There is no deleted/deleted case because that's not a conflict.

  local_data.specifics.mutable_preference()->set_value("value");
  EXPECT_FALSE(local_data.is_deleted());
  EXPECT_TRUE(remote_data.is_deleted());
  EXPECT_EQ(ConflictResolution::USE_LOCAL,
            bridge()->ResolveConflict(local_data, remote_data).type());

  remote_data.specifics.mutable_preference()->set_value("value");
  EXPECT_FALSE(local_data.is_deleted());
  EXPECT_FALSE(remote_data.is_deleted());
  EXPECT_EQ(ConflictResolution::USE_REMOTE,
            bridge()->ResolveConflict(local_data, remote_data).type());

  local_data.specifics.clear_preference();
  EXPECT_TRUE(local_data.is_deleted());
  EXPECT_FALSE(remote_data.is_deleted());
  EXPECT_EQ(ConflictResolution::USE_REMOTE,
            bridge()->ResolveConflict(local_data, remote_data).type());
}

}  // namespace syncer
