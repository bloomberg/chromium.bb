// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/non_blocking_data_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/api/fake_model_type_change_processor.h"
#include "components/sync/api/stub_model_type_service.h"
#include "components/sync/driver/fake_sync_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
class SyncClient;
}  // namespace syncer

namespace syncer {

namespace {

ModelType kTestModelType = AUTOFILL;

// Implementation of NonBlockingDataTypeController being tested.
// It posts all tasks to current thread.
class TestDataTypeController : public NonBlockingDataTypeController {
 public:
  explicit TestDataTypeController(SyncClient* sync_client)
      : NonBlockingDataTypeController(kTestModelType,
                                      base::Closure(),
                                      sync_client) {}
  ~TestDataTypeController() override {}

 protected:
  bool RunOnModelThread(const tracked_objects::Location& from_here,
                        const base::Closure& task) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(from_here, task);
    return true;
  }
};

// Mock change processor to observe calls to DisableSync.
class MockModelTypeChangeProcessor : public FakeModelTypeChangeProcessor {
 public:
  explicit MockModelTypeChangeProcessor(int* disable_sync_call_count)
      : disable_sync_call_count_(disable_sync_call_count) {}

  void DisableSync() override { (*disable_sync_call_count_)++; }

 private:
  int* disable_sync_call_count_;
};

class NonBlockingDataTypeControllerTest : public testing::Test {
 public:
  NonBlockingDataTypeControllerTest()
      : disable_sync_call_count_(0),
        sync_prefs_(sync_client_.GetPrefService()),
        model_type_service_(
            base::Bind(&NonBlockingDataTypeControllerTest::CreateProcessor,
                       base::Unretained(this))),
        controller_(&sync_client_) {}

  void SetUp() override {
    sync_client_.SetModelTypeService(&model_type_service_);
  }

  void TearDown() override { PumpLoop(); }

 protected:
  std::unique_ptr<ModelTypeChangeProcessor> CreateProcessor(
      ModelType type,
      ModelTypeService* service) {
    auto processor = base::MakeUnique<MockModelTypeChangeProcessor>(
        &disable_sync_call_count_);
    processor_ = processor.get();
    return std::move(processor);
  }

  // Gets controller from NOT_RUNNING to RUNNING state.
  void ActivateController() {
    EXPECT_EQ(DataTypeController::NOT_RUNNING, controller_.state());
    controller_.LoadModels(
        base::Bind(&NonBlockingDataTypeControllerTest::LoadModelsDone,
                   base::Unretained(this)));

    EXPECT_EQ(DataTypeController::MODEL_STARTING, controller_.state());
    PumpLoop();
    EXPECT_EQ(DataTypeController::MODEL_LOADED, controller_.state());
    controller_.StartAssociating(
        base::Bind(&NonBlockingDataTypeControllerTest::AssociationDone,
                   base::Unretained(this)));
    EXPECT_EQ(DataTypeController::RUNNING, controller_.state());
  }

  void LoadModelsDone(ModelType type, const SyncError& error) {}

  void AssociationDone(DataTypeController::ConfigureResult result,
                       const SyncMergeResult& local_merge_result,
                       const SyncMergeResult& syncer_merge_result) {}

  void PumpLoop() { base::RunLoop().RunUntilIdle(); }

  int disable_sync_call_count_;
  base::MessageLoop message_loop_;
  FakeSyncClient sync_client_;
  SyncPrefs sync_prefs_;
  MockModelTypeChangeProcessor* processor_;
  StubModelTypeService model_type_service_;
  TestDataTypeController controller_;
};

// Test emulates normal browser shutdown. Ensures that DisableSync is not
// called.
TEST_F(NonBlockingDataTypeControllerTest, StopWhenDatatypeEnabled) {
  // Enable datatype through preferences.
  sync_prefs_.SetFirstSetupComplete();
  sync_prefs_.SetKeepEverythingSynced(true);

  ActivateController();

  controller_.Stop();
  PumpLoop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller_.state());
  // Ensure that DisableSync is not called and service still has valid change
  // processor.
  EXPECT_EQ(0, disable_sync_call_count_);
  EXPECT_TRUE(model_type_service_.HasChangeProcessor());
}

// Test emulates scenario when user disables datatype. DisableSync should be
// called.
TEST_F(NonBlockingDataTypeControllerTest, StopWhenDatatypeDisabled) {
  // Enable datatype through preferences.
  sync_prefs_.SetFirstSetupComplete();
  sync_prefs_.SetKeepEverythingSynced(true);
  ActivateController();

  // Disable datatype through preferences.
  sync_prefs_.SetKeepEverythingSynced(false);
  sync_prefs_.SetPreferredDataTypes(ModelTypeSet(kTestModelType),
                                    ModelTypeSet());

  controller_.Stop();
  PumpLoop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller_.state());
  // Ensure that DisableSync is called and change processor is reset.
  EXPECT_EQ(1, disable_sync_call_count_);
  EXPECT_FALSE(model_type_service_.HasChangeProcessor());
}

// Test emulates disabling sync by signing out. DisableSync should be called.
TEST_F(NonBlockingDataTypeControllerTest, StopWithInitialSyncPrefs) {
  // Enable datatype through preferences.
  sync_prefs_.SetFirstSetupComplete();
  sync_prefs_.SetKeepEverythingSynced(true);
  ActivateController();

  // Clearing preferences emulates signing out.
  sync_prefs_.ClearPreferences();
  controller_.Stop();
  PumpLoop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller_.state());
  // Ensure that DisableSync is called and change processor is reset.
  EXPECT_EQ(1, disable_sync_call_count_);
  EXPECT_FALSE(model_type_service_.HasChangeProcessor());
}

// Test emulates disabling sync when datatype is not loaded yet. DisableSync
// should not be called as service is potentially not ready to handle it.
TEST_F(NonBlockingDataTypeControllerTest, StopBeforeLoadModels) {
  // Enable datatype through preferences.
  sync_prefs_.SetFirstSetupComplete();
  sync_prefs_.SetKeepEverythingSynced(true);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller_.state());

  // Clearing preferences emulates signing out.
  sync_prefs_.ClearPreferences();
  controller_.Stop();
  PumpLoop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller_.state());
  // Ensure that DisableSync is not called.
  EXPECT_EQ(0, disable_sync_call_count_);
}

}  // namespace

}  // namespace syncer
