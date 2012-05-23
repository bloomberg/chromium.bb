// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/sync/glue/fake_data_type_controller.h"
#include "chrome/browser/sync/glue/model_association_manager.h"
#include "content/test/test_browser_thread.h"
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
};

FakeDataTypeController* GetController(
    const DataTypeController::TypeMap& controllers,
    syncable::ModelType model_type) {
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
  EXPECT_EQ(arg0.errors.size(), expected_result.errors.size());

  if (arg0.errors.size() == expected_result.errors.size()) {
    std::list<SyncError>::const_iterator it1, it2;
    for (it1 = arg0.errors.begin(),
         it2 = expected_result.errors.begin();
         it1 != arg0.errors.end();
         ++it1, ++it2) {
      EXPECT_EQ((*it1).type(), (*it2).type());
    }
  }
}

class ModelAssociationManagerTest : public testing::Test {
 public:
  ModelAssociationManagerTest() :
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
TEST_F(ModelAssociationManagerTest, SimpleModelStart) {
  controllers_[syncable::BOOKMARKS] =
      new FakeDataTypeController(syncable::BOOKMARKS);
  ModelAssociationManager model_association_manager(&controllers_,
                                                    &result_processor_);
  syncable::ModelTypeSet types;
  types.Put(syncable::BOOKMARKS);
  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::OK,
      types,
      std::list<SyncError>());
  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  model_association_manager.Initialize(types);
  model_association_manager.StopDisabledTypes();
  model_association_manager.StartAssociationAsync();

  EXPECT_EQ(GetController(controllers_, syncable::BOOKMARKS)->state(),
            DataTypeController::MODEL_STARTING);
  GetController(controllers_, syncable::BOOKMARKS)->FinishStart(
      DataTypeController::OK);
}

// Start a type and call stop before it finishes associating.
TEST_F(ModelAssociationManagerTest, StopModelBeforeFinish) {
  controllers_[syncable::BOOKMARKS] =
      new FakeDataTypeController(syncable::BOOKMARKS);
  ModelAssociationManager model_association_manager(&controllers_,
                                                    &result_processor_);

  syncable::ModelTypeSet types;
  types.Put(syncable::BOOKMARKS);

  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::ABORTED,
      types,
      std::list<SyncError>());

  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  model_association_manager.Initialize(types);
  model_association_manager.StopDisabledTypes();
  model_association_manager.StartAssociationAsync();

  EXPECT_EQ(GetController(controllers_, syncable::BOOKMARKS)->state(),
            DataTypeController::MODEL_STARTING);
  model_association_manager.Stop();
  EXPECT_EQ(GetController(controllers_, syncable::BOOKMARKS)->state(),
            DataTypeController::NOT_RUNNING);
}

// Start a type, let it finish and then call stop.
TEST_F(ModelAssociationManagerTest, StopAfterFinish) {
  controllers_[syncable::BOOKMARKS] =
      new FakeDataTypeController(syncable::BOOKMARKS);
  ModelAssociationManager model_association_manager(&controllers_,
                                                    &result_processor_);
  syncable::ModelTypeSet types;
  types.Put(syncable::BOOKMARKS);
  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::OK,
      types,
      std::list<SyncError>());
  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  model_association_manager.Initialize(types);
  model_association_manager.StopDisabledTypes();
  model_association_manager.StartAssociationAsync();

  EXPECT_EQ(GetController(controllers_, syncable::BOOKMARKS)->state(),
            DataTypeController::MODEL_STARTING);
  GetController(controllers_, syncable::BOOKMARKS)->FinishStart(
      DataTypeController::OK);

  model_association_manager.Stop();
  EXPECT_EQ(GetController(controllers_, syncable::BOOKMARKS)->state(),
            DataTypeController::NOT_RUNNING);
}

// Make a type fail model association and verify correctness.
TEST_F(ModelAssociationManagerTest, TypeFailModelAssociation) {
  controllers_[syncable::BOOKMARKS] =
      new FakeDataTypeController(syncable::BOOKMARKS);
  ModelAssociationManager model_association_manager(&controllers_,
                                                    &result_processor_);
  syncable::ModelTypeSet types;
  types.Put(syncable::BOOKMARKS);
  std::list<SyncError> errors;
  SyncError error(FROM_HERE, "Failed", syncable::BOOKMARKS);
  errors.push_back(error);
  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::PARTIAL_SUCCESS,
      types,
      errors);
  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  model_association_manager.Initialize(types);
  model_association_manager.StopDisabledTypes();
  model_association_manager.StartAssociationAsync();

  EXPECT_EQ(GetController(controllers_, syncable::BOOKMARKS)->state(),
            DataTypeController::MODEL_STARTING);
  GetController(controllers_, syncable::BOOKMARKS)->FinishStart(
      DataTypeController::ASSOCIATION_FAILED);
}

// Ensure configuring stops when a type returns a unrecoverable error.
TEST_F(ModelAssociationManagerTest, TypeReturnUnrecoverableError) {
  controllers_[syncable::BOOKMARKS] =
      new FakeDataTypeController(syncable::BOOKMARKS);
  ModelAssociationManager model_association_manager(&controllers_,
                                                    &result_processor_);
  syncable::ModelTypeSet types;
  types.Put(syncable::BOOKMARKS);
  std::list<SyncError> errors;
  SyncError error(FROM_HERE, "Failed", syncable::BOOKMARKS);
  errors.push_back(error);
  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::UNRECOVERABLE_ERROR,
      types,
      errors);
  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  model_association_manager.Initialize(types);
  model_association_manager.StopDisabledTypes();
  model_association_manager.StartAssociationAsync();

  EXPECT_EQ(GetController(controllers_, syncable::BOOKMARKS)->state(),
            DataTypeController::MODEL_STARTING);
  GetController(controllers_, syncable::BOOKMARKS)->FinishStart(
      DataTypeController::UNRECOVERABLE_ERROR);
}

}  // namespace browser_sync
