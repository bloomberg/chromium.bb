// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "components/sync_driver/non_blocking_data_type_controller.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/non_blocking_type_processor.h"
#include "sync/internal_api/public/sync_core_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockSyncCoreProxy : public syncer::SyncCoreProxy {
 public:
  MockSyncCoreProxy() {}
  virtual ~MockSyncCoreProxy() {}

  virtual void ConnectTypeToCore(
      syncer::ModelType type,
      base::WeakPtr<syncer::NonBlockingTypeProcessor> type_processor) OVERRIDE {
    type_processor->OnConnect(
        base::WeakPtr<syncer::NonBlockingTypeProcessorCore>(),
        scoped_refptr<base::SequencedTaskRunner>());
  }

  virtual scoped_ptr<SyncCoreProxy> Clone() OVERRIDE {
    return scoped_ptr<SyncCoreProxy>(new MockSyncCoreProxy());
  }
};

class NonBlockingDataTypeControllerTest : public testing::Test {
 public:
  NonBlockingDataTypeControllerTest()
      : processor_(syncer::DICTIONARY),
        model_thread_(new base::TestSimpleTaskRunner()),
        controller_(syncer::DICTIONARY, true) {}

  virtual ~NonBlockingDataTypeControllerTest() {}

  // Connects the processor to the NonBlockingDataTypeController.
  void InitProcessor() {
    controller_.InitializeProcessor(
        model_thread_, processor_.AsWeakPtr());
    RunQueuedTasks();
  }

  // Connects the sync backend to the NonBlockingDataTypeController.
  void InitSync() {
    controller_.InitializeSyncCoreProxy(mock_core_proxy_.Clone());
    RunQueuedTasks();
  }

  // Disconnects the sync backend from the NonBlockingDataTypeController.
  void UninitializeSync() {
    controller_.ClearSyncCoreProxy();
    RunQueuedTasks();
  }

  // Toggles the user's preference for syncing this type.
  void SetIsPreferred(bool preferred) {
    controller_.SetIsPreferred(preferred);
    RunQueuedTasks();
  }

  // The processor pretends to run tasks on a different thread.
  // This function runs any posted tasks.
  void RunQueuedTasks() {
    model_thread_->RunUntilIdle();
  }

 protected:
  MockSyncCoreProxy mock_core_proxy_;

  syncer::NonBlockingTypeProcessor processor_;
  scoped_refptr<base::TestSimpleTaskRunner> model_thread_;

  browser_sync::NonBlockingDataTypeController controller_;
};

// Initialization when the user has disabled syncing for this type.
TEST_F(NonBlockingDataTypeControllerTest, UserDisabled) {
  SetIsPreferred(false);
  InitProcessor();
  InitSync();

  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());

  UninitializeSync();

  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());
}

// Init the sync backend then the type processor.
TEST_F(NonBlockingDataTypeControllerTest, Enabled_SyncFirst) {
  SetIsPreferred(true);
  InitSync();
  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());

  InitProcessor();
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_TRUE(processor_.IsConnected());

  UninitializeSync();
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());
}

// Init the type processor then the sync backend.
TEST_F(NonBlockingDataTypeControllerTest, Enabled_ProcessorFirst) {
  SetIsPreferred(true);
  InitProcessor();
  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());

  InitSync();
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_TRUE(processor_.IsConnected());

  UninitializeSync();
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());
}

// Initialize sync then disable it with a pref change.
TEST_F(NonBlockingDataTypeControllerTest, PreferThenNot) {
  SetIsPreferred(true);
  InitProcessor();
  InitSync();

  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_TRUE(processor_.IsConnected());

  SetIsPreferred(false);
  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());
}

// Connect type processor and sync backend, then toggle prefs repeatedly.
TEST_F(NonBlockingDataTypeControllerTest, RepeatedTogglePreference) {
  SetIsPreferred(false);
  InitProcessor();
  InitSync();
  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());

  SetIsPreferred(true);
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_TRUE(processor_.IsConnected());

  SetIsPreferred(false);
  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());

  SetIsPreferred(true);
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_TRUE(processor_.IsConnected());

  SetIsPreferred(false);
  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());
}

// Test sync backend getting restarted while processor is connected.
TEST_F(NonBlockingDataTypeControllerTest, RestartSyncBackend) {
  SetIsPreferred(true);
  InitProcessor();
  InitSync();
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_TRUE(processor_.IsConnected());

  // Shutting down sync backend should disconnect but not disable the type.
  UninitializeSync();
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());

  // Brining the backend back should reconnect the type.
  InitSync();
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_TRUE(processor_.IsConnected());
}

// Test sync backend being restarted before processor connects.
TEST_F(NonBlockingDataTypeControllerTest, RestartSyncBackendEarly) {
  SetIsPreferred(true);

  // Toggle sync off and on before the type processor is available.
  InitSync();
  EXPECT_FALSE(processor_.IsConnected());
  UninitializeSync();
  EXPECT_FALSE(processor_.IsConnected());
  InitSync();
  EXPECT_FALSE(processor_.IsConnected());

  // Introduce the processor.
  InitProcessor();
  EXPECT_TRUE(processor_.IsConnected());
}

// Test pref toggling before the sync backend has connected.
TEST_F(NonBlockingDataTypeControllerTest, TogglePreferenceWithoutBackend) {
  SetIsPreferred(true);
  InitProcessor();

  // This should emit a disable signal.
  SetIsPreferred(false);
  EXPECT_FALSE(processor_.IsConnected());
  EXPECT_FALSE(processor_.IsPreferred());

  // This won't enable us, since we don't have a sync backend.
  SetIsPreferred(true);
  EXPECT_FALSE(processor_.IsConnected());
  EXPECT_FALSE(processor_.IsPreferred());

  // Only now do we start sending enable signals.
  InitSync();
  EXPECT_TRUE(processor_.IsConnected());
  EXPECT_TRUE(processor_.IsPreferred());
}

}  // namespace
