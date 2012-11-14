// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/glue/fake_data_type_controller.h"
#include "chrome/browser/sync/glue/model_association_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
namespace browser_sync {
class MockModelAssociationResultProcessor :
    public ModelAssociationResultProcessor {
 public:
  MockModelAssociationResultProcessor() {}
  ~MockModelAssociationResultProcessor() {}
  MOCK_METHOD1(OnModelAssociationDone, void(
      const DataTypeManager::ConfigureResult& result));
  MOCK_METHOD0(OnTypesLoaded, void());
};

FakeDataTypeController* GetController(
    const DataTypeController::TypeMap& controllers,
    syncer::ModelType model_type) {
  DataTypeController::TypeMap::const_iterator it =
      controllers.find(model_type);
  if (it == controllers.end()) {
    return NULL;
  }
  return (FakeDataTypeController*)(it->second.get());
}

ACTION_P(VerifyResult, expected_result) {
  EXPECT_EQ(arg0.status, expected_result.status);
  EXPECT_TRUE(arg0.requested_types.Equals(expected_result.requested_types));
  EXPECT_EQ(arg0.failed_data_types.size(),
            expected_result.failed_data_types.size());

  if (arg0.failed_data_types.size() ==
          expected_result.failed_data_types.size()) {
    std::list<syncer::SyncError>::const_iterator it1, it2;
    for (it1 = arg0.failed_data_types.begin(),
         it2 = expected_result.failed_data_types.begin();
         it1 != arg0.failed_data_types.end();
         ++it1, ++it2) {
      EXPECT_EQ((*it1).type(), (*it2).type());
    }
  }

  EXPECT_TRUE(arg0.waiting_to_start.Equals(expected_result.waiting_to_start));
}

class SyncModelAssociationManagerTest : public testing::Test {
 public:
  SyncModelAssociationManagerTest() :
      ui_thread_(content::BrowserThread::UI, &ui_loop_) {
  }

 protected:
  MessageLoopForUI ui_loop_;
  content::TestBrowserThread ui_thread_;
  MockModelAssociationResultProcessor result_processor_;
  DataTypeController::TypeMap controllers_;
};

// Start a type and make sure ModelAssociationManager callst the |Start|
// method and calls the callback when it is done.
TEST_F(SyncModelAssociationManagerTest, SimpleModelStart) {
  controllers_[syncer::BOOKMARKS] =
      new FakeDataTypeController(syncer::BOOKMARKS);
  ModelAssociationManager model_association_manager(
      syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(),
      &controllers_,
      &result_processor_);
  syncer::ModelTypeSet types;
  types.Put(syncer::BOOKMARKS);
  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::OK,
      types,
      std::list<syncer::SyncError>(),
      syncer::ModelTypeSet());
  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  model_association_manager.Initialize(types);
  model_association_manager.StopDisabledTypes();
  model_association_manager.StartAssociationAsync();

  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  GetController(controllers_, syncer::BOOKMARKS)->FinishStart(
      DataTypeController::OK);
}

// Start a type and call stop before it finishes associating.
TEST_F(SyncModelAssociationManagerTest, StopModelBeforeFinish) {
  controllers_[syncer::BOOKMARKS] =
      new FakeDataTypeController(syncer::BOOKMARKS);
  ModelAssociationManager model_association_manager(
      syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(),
      &controllers_,
      &result_processor_);

  syncer::ModelTypeSet types;
  types.Put(syncer::BOOKMARKS);

  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::ABORTED,
      types,
      std::list<syncer::SyncError>(),
      syncer::ModelTypeSet());

  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  model_association_manager.Initialize(types);
  model_association_manager.StopDisabledTypes();
  model_association_manager.StartAssociationAsync();

  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  model_association_manager.Stop();
  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::NOT_RUNNING);
}

// Start a type, let it finish and then call stop.
TEST_F(SyncModelAssociationManagerTest, StopAfterFinish) {
  controllers_[syncer::BOOKMARKS] =
      new FakeDataTypeController(syncer::BOOKMARKS);
  ModelAssociationManager model_association_manager(
      syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(),
      &controllers_,
      &result_processor_);
  syncer::ModelTypeSet types;
  types.Put(syncer::BOOKMARKS);
  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::OK,
      types,
      std::list<syncer::SyncError>(),
      syncer::ModelTypeSet());
  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  model_association_manager.Initialize(types);
  model_association_manager.StopDisabledTypes();
  model_association_manager.StartAssociationAsync();

  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  GetController(controllers_, syncer::BOOKMARKS)->FinishStart(
      DataTypeController::OK);

  model_association_manager.Stop();
  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::NOT_RUNNING);
}

// Make a type fail model association and verify correctness.
TEST_F(SyncModelAssociationManagerTest, TypeFailModelAssociation) {
  controllers_[syncer::BOOKMARKS] =
      new FakeDataTypeController(syncer::BOOKMARKS);
  ModelAssociationManager model_association_manager(
      syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(),
      &controllers_,
      &result_processor_);
  syncer::ModelTypeSet types;
  types.Put(syncer::BOOKMARKS);
  std::list<syncer::SyncError> errors;
  syncer::SyncError error(FROM_HERE, "Failed", syncer::BOOKMARKS);
  errors.push_back(error);
  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::PARTIAL_SUCCESS,
      types,
      errors,
      syncer::ModelTypeSet());
  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  model_association_manager.Initialize(types);
  model_association_manager.StopDisabledTypes();
  model_association_manager.StartAssociationAsync();

  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  GetController(controllers_, syncer::BOOKMARKS)->FinishStart(
      DataTypeController::ASSOCIATION_FAILED);
}

// Ensure configuring stops when a type returns a unrecoverable error.
TEST_F(SyncModelAssociationManagerTest, TypeReturnUnrecoverableError) {
  controllers_[syncer::BOOKMARKS] =
      new FakeDataTypeController(syncer::BOOKMARKS);
  ModelAssociationManager model_association_manager(
      syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(),
      &controllers_,
      &result_processor_);
  syncer::ModelTypeSet types;
  types.Put(syncer::BOOKMARKS);
  std::list<syncer::SyncError> errors;
  syncer::SyncError error(FROM_HERE, "Failed", syncer::BOOKMARKS);
  errors.push_back(error);
  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::UNRECOVERABLE_ERROR,
      types,
      errors,
      syncer::ModelTypeSet());
  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  model_association_manager.Initialize(types);
  model_association_manager.StopDisabledTypes();
  model_association_manager.StartAssociationAsync();

  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  GetController(controllers_, syncer::BOOKMARKS)->FinishStart(
      DataTypeController::UNRECOVERABLE_ERROR);
}

TEST_F(SyncModelAssociationManagerTest, InitializeAbortsLoad) {
  controllers_[syncer::BOOKMARKS] =
      new FakeDataTypeController(syncer::BOOKMARKS);
  controllers_[syncer::THEMES] =
      new FakeDataTypeController(syncer::THEMES);

  GetController(controllers_, syncer::BOOKMARKS)->SetDelayModelLoad();
  ModelAssociationManager model_association_manager(
      syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(),
      &controllers_,
      &result_processor_);
  syncer::ModelTypeSet types(syncer::BOOKMARKS, syncer::THEMES);

  syncer::ModelTypeSet expected_types_waiting_to_load;
  expected_types_waiting_to_load.Put(syncer::BOOKMARKS);
  DataTypeManager::ConfigureResult expected_result_partially_done(
      DataTypeManager::PARTIAL_SUCCESS,
      types,
      std::list<syncer::SyncError>(),
      expected_types_waiting_to_load);

  model_association_manager.Initialize(types);
  model_association_manager.StopDisabledTypes();

  model_association_manager.StartAssociationAsync();

  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result_partially_done));

  base::OneShotTimer<ModelAssociationManager>* timer =
      model_association_manager.GetTimerForTesting();

  base::Closure task = timer->user_task();
  timer->Stop();
  task.Run();  // Bookmark load times out here.

  // Apps finishes associating here.
  GetController(controllers_, syncer::THEMES)->FinishStart(
      DataTypeController::OK);

  // At this point, BOOKMARKS is still waiting to load (as evidenced by
  // expected_result_partially_done). If we schedule another Initialize (which
  // could happen in practice due to reconfiguration), this should abort
  // BOOKMARKS. Aborting will call ModelLoadCallback, but the
  // ModelAssociationManager should be smart enough to know that this is not due
  // to the type having completed loading.
  EXPECT_CALL(result_processor_, OnTypesLoaded()).Times(0);

  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::MODEL_STARTING);

  model_association_manager.Initialize(types);
  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::NOT_RUNNING);

  DataTypeManager::ConfigureResult expected_result_done(
      DataTypeManager::OK,
      types,
      std::list<syncer::SyncError>(),
      syncer::ModelTypeSet());
  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result_done));

  model_association_manager.StopDisabledTypes();
  model_association_manager.StartAssociationAsync();

  GetController(controllers_,
                syncer::BOOKMARKS)->SimulateModelLoadFinishing();
  GetController(controllers_, syncer::BOOKMARKS)->FinishStart(
      DataTypeController::OK);
}

// Start 2 types. One of which timeout loading. Ensure that type is
// fully configured eventually.
TEST_F(SyncModelAssociationManagerTest, ModelStartWithSlowLoadingType) {
  controllers_[syncer::BOOKMARKS] =
      new FakeDataTypeController(syncer::BOOKMARKS);
  controllers_[syncer::APPS] =
      new FakeDataTypeController(syncer::APPS);
  GetController(controllers_, syncer::BOOKMARKS)->SetDelayModelLoad();
  ModelAssociationManager model_association_manager(
      syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(),
      &controllers_,
      &result_processor_);
  syncer::ModelTypeSet types;
  types.Put(syncer::BOOKMARKS);
  types.Put(syncer::APPS);

  syncer::ModelTypeSet expected_types_waiting_to_load;
  expected_types_waiting_to_load.Put(syncer::BOOKMARKS);
  DataTypeManager::ConfigureResult expected_result_partially_done(
      DataTypeManager::PARTIAL_SUCCESS,
      types,
      std::list<syncer::SyncError>(),
      expected_types_waiting_to_load);

  DataTypeManager::ConfigureResult expected_result_done(
      DataTypeManager::OK,
      types,
      std::list<syncer::SyncError>(),
      syncer::ModelTypeSet());

  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result_partially_done));
  EXPECT_CALL(result_processor_, OnTypesLoaded());

  model_association_manager.Initialize(types);
  model_association_manager.StopDisabledTypes();
  model_association_manager.StartAssociationAsync();

  base::OneShotTimer<ModelAssociationManager>* timer =
      model_association_manager.GetTimerForTesting();

  // Note: Independent of the timeout value this test is not flaky.
  // The reason is timer posts a task which would never be executed
  // as we dont let the message loop run.
  base::Closure task = timer->user_task();
  timer->Stop();
  task.Run();

  // Simulate delayed loading of bookmark model.
  GetController(controllers_, syncer::APPS)->FinishStart(
      DataTypeController::OK);

  GetController(controllers_,
                syncer::BOOKMARKS)->SimulateModelLoadFinishing();

  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result_done));

  // Do it once more to associate bookmarks.
  model_association_manager.Initialize(types);
  model_association_manager.StopDisabledTypes();
  model_association_manager.StartAssociationAsync();

  GetController(controllers_,
                syncer::BOOKMARKS)->SimulateModelLoadFinishing();

  GetController(controllers_, syncer::BOOKMARKS)->FinishStart(
      DataTypeController::OK);
}


}  // namespace browser_sync
