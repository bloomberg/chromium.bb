// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/non_blocking_data_type_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/sync_driver/fake_sync_client.h"
#include "components/sync_driver/sync_prefs.h"
#include "sync/api/fake_model_type_change_processor.h"
#include "sync/api/fake_model_type_service.h"
#include "sync/internal_api/public/base/model_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_driver {
class SyncClient;
}  // namespace sync_driver

namespace sync_driver_v2 {

namespace {

syncer::ModelType kTestModelType = syncer::AUTOFILL;

// Implementation of NonBlockingDataTypeController being tested.
// It posts all tasks to current thread.
class TestDataTypeController : public NonBlockingDataTypeController {
 public:
  TestDataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& thread,
      sync_driver::SyncClient* sync_client)
      : NonBlockingDataTypeController(thread,
                                      base::Closure(),
                                      kTestModelType,
                                      sync_client) {}

 protected:
  ~TestDataTypeController() override {}

  bool RunOnModelThread(const tracked_objects::Location& from_here,
                        const base::Closure& task) override {
    ui_thread()->PostTask(from_here, task);
    return true;
  }

  void RunOnUIThread(const tracked_objects::Location& from_here,
                     const base::Closure& task) override {
    ui_thread()->PostTask(from_here, task);
  }
};

// Mock change processor to observe calls to DisableSync.
class MockModelTypeChangeProcessor
    : public syncer_v2::FakeModelTypeChangeProcessor {
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
                       base::Unretained(this))) {}

  void SetUp() override {
    sync_client_.SetModelTypeService(&model_type_service_);
    controller_ =
        new TestDataTypeController(message_loop_.task_runner(), &sync_client_);
  }

  void TearDown() override {
    controller_ = nullptr;
    PumpLoop();
  }

 protected:
  std::unique_ptr<syncer_v2::ModelTypeChangeProcessor> CreateProcessor(
      syncer::ModelType type,
      syncer_v2::ModelTypeService* service) {
    auto processor = base::MakeUnique<MockModelTypeChangeProcessor>(
        &disable_sync_call_count_);
    processor_ = processor.get();
    return std::move(processor);
  }

  // Gets controller from NOT_RUNNING to RUNNING state.
  void ActivateController() {
    EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING,
              controller_->state());
    controller_->LoadModels(
        base::Bind(&NonBlockingDataTypeControllerTest::LoadModelsDone,
                   base::Unretained(this)));

    EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
              controller_->state());
    PumpLoop();
    EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
              controller_->state());
    controller_->StartAssociating(
        base::Bind(&NonBlockingDataTypeControllerTest::AssociationDone,
                   base::Unretained(this)));
    EXPECT_EQ(sync_driver::DataTypeController::RUNNING, controller_->state());
  }

  void LoadModelsDone(syncer::ModelType type, syncer::SyncError error) {}

  void AssociationDone(sync_driver::DataTypeController::ConfigureResult result,
                       const syncer::SyncMergeResult& local_merge_result,
                       const syncer::SyncMergeResult& syncer_merge_result) {}

  void PumpLoop() { base::RunLoop().RunUntilIdle(); }

  int disable_sync_call_count_;
  base::MessageLoop message_loop_;
  sync_driver::FakeSyncClient sync_client_;
  sync_driver::SyncPrefs sync_prefs_;
  MockModelTypeChangeProcessor* processor_;
  syncer_v2::FakeModelTypeService model_type_service_;
  scoped_refptr<TestDataTypeController> controller_;
};

// Test emulates normal browser shutdown. Ensures that DisableSync is not
// called.
TEST_F(NonBlockingDataTypeControllerTest, StopWhenDatatypeEnabled) {
  // Enable datatype through preferences.
  sync_prefs_.SetFirstSetupComplete();
  sync_prefs_.SetKeepEverythingSynced(true);

  ActivateController();

  controller_->Stop();
  PumpLoop();
  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING, controller_->state());
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
  sync_prefs_.SetPreferredDataTypes(syncer::ModelTypeSet(kTestModelType),
                                    syncer::ModelTypeSet());

  controller_->Stop();
  PumpLoop();
  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING, controller_->state());
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
  controller_->Stop();
  PumpLoop();
  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING, controller_->state());
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
  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING, controller_->state());

  // Clearing preferences emulates signing out.
  sync_prefs_.ClearPreferences();
  controller_->Stop();
  PumpLoop();
  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING, controller_->state());
  // Ensure that DisableSync is not called.
  EXPECT_EQ(0, disable_sync_call_count_);
}

}  // namespace

}  // namespace sync_driver_v2
