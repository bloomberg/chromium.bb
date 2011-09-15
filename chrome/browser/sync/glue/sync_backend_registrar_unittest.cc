// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_backend_registrar.h"

#include "chrome/browser/sync/glue/change_processor_mock.h"
#include "chrome/browser/sync/glue/ui_model_worker.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/test/engine/test_user_share.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
using syncable::FIRST_REAL_MODEL_TYPE;
using syncable::AUTOFILL;
using syncable::BOOKMARKS;
using syncable::PREFERENCES;
using syncable::THEMES;
using syncable::NIGORI;
using syncable::PASSWORDS;
using syncable::MODEL_TYPE_COUNT;
using syncable::ModelType;
using syncable::ModelTypeFromInt;
using syncable::ModelTypeSet;

class SyncBackendRegistrarTest : public testing::Test {
 protected:
  SyncBackendRegistrarTest() : ui_thread_(BrowserThread::UI, &loop_) {}

  virtual ~SyncBackendRegistrarTest() {}

  virtual void SetUp() {
    test_user_share_.SetUp();
  }

  virtual void TearDown() {
    test_user_share_.TearDown();
  }

  void ExpectRoutingInfo(SyncBackendRegistrar* registrar,
                         const ModelSafeRoutingInfo& expected_routing_info) {
    ModelSafeRoutingInfo routing_info;
    registrar->GetModelSafeRoutingInfo(&routing_info);
    EXPECT_EQ(expected_routing_info, routing_info);
  }

  void ExpectHasProcessorsForTypes(SyncBackendRegistrar* registrar,
                                   const ModelTypeSet& types) {
    for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
      ModelType model_type = ModelTypeFromInt(i);
      if (types.count(model_type) > 0) {
        EXPECT_TRUE(registrar->GetProcessor(model_type));
      } else {
        EXPECT_FALSE(registrar->GetProcessor(model_type));
      }
    }
  }

  MessageLoop loop_;
  TestUserShare test_user_share_;

 private:
  BrowserThread ui_thread_;
};

TEST_F(SyncBackendRegistrarTest, ConstructorEmpty) {
  TestingProfile profile;
  SyncBackendRegistrar registrar(ModelTypeSet(), "test", &profile, &loop_);
  EXPECT_FALSE(registrar.IsNigoriEnabled());
  {
    std::vector<ModelSafeWorker*> workers;
    registrar.GetWorkers(&workers);
    EXPECT_EQ(4u, workers.size());
  }
  ExpectRoutingInfo(&registrar, ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(&registrar, ModelTypeSet());
  registrar.OnSyncerShutdownComplete();
  registrar.StopOnUIThread();
}

TEST_F(SyncBackendRegistrarTest, ConstructorNonEmpty) {
  TestingProfile profile;
  ModelTypeSet initial_types;
  initial_types.insert(BOOKMARKS);
  initial_types.insert(NIGORI);
  initial_types.insert(PASSWORDS);
  SyncBackendRegistrar registrar(initial_types, "test", &profile, &loop_);
  EXPECT_TRUE(registrar.IsNigoriEnabled());
  {
    std::vector<ModelSafeWorker*> workers;
    registrar.GetWorkers(&workers);
    EXPECT_EQ(4u, workers.size());
  }
  {
    ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[BOOKMARKS] = GROUP_PASSIVE;
    expected_routing_info[NIGORI] = GROUP_PASSIVE;
    // Passwords dropped because of no password store.
    ExpectRoutingInfo(&registrar, expected_routing_info);
  }
  ExpectHasProcessorsForTypes(&registrar, ModelTypeSet());
  registrar.OnSyncerShutdownComplete();
  registrar.StopOnUIThread();
}

TEST_F(SyncBackendRegistrarTest, ConfigureDataTypes) {
  TestingProfile profile;
  SyncBackendRegistrar registrar(ModelTypeSet(), "test", &profile, &loop_);

  // Add.
  ModelTypeSet types1;
  types1.insert(BOOKMARKS);
  types1.insert(NIGORI);
  types1.insert(AUTOFILL);
  EXPECT_EQ(types1, registrar.ConfigureDataTypes(types1, ModelTypeSet()));
  {
    ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[BOOKMARKS] = GROUP_PASSIVE;
    expected_routing_info[NIGORI] = GROUP_PASSIVE;
    expected_routing_info[AUTOFILL] = GROUP_PASSIVE;
    ExpectRoutingInfo(&registrar, expected_routing_info);
  }
  ExpectHasProcessorsForTypes(&registrar, ModelTypeSet());

  // Add and remove.
  ModelTypeSet types2;
  types2.insert(PREFERENCES);
  types2.insert(THEMES);
  EXPECT_EQ(types2, registrar.ConfigureDataTypes(types2, types1));
  {
    ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[PREFERENCES] = GROUP_PASSIVE;
    expected_routing_info[THEMES] = GROUP_PASSIVE;
    ExpectRoutingInfo(&registrar, expected_routing_info);
  }
  ExpectHasProcessorsForTypes(&registrar, ModelTypeSet());

  // Remove.
  EXPECT_TRUE(registrar.ConfigureDataTypes(ModelTypeSet(), types2).empty());
  ExpectRoutingInfo(&registrar, ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(&registrar, ModelTypeSet());

  registrar.OnSyncerShutdownComplete();
  registrar.StopOnUIThread();
}

TEST_F(SyncBackendRegistrarTest, ActivateDeactivateUIDataType) {
  InSequence in_sequence;
  TestingProfile profile;
  SyncBackendRegistrar registrar(ModelTypeSet(), "test", &profile, &loop_);
  StrictMock<ChangeProcessorMock> change_processor_mock;
  EXPECT_CALL(change_processor_mock, StartImpl(&profile));
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, StopImpl());
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(false));

  ModelTypeSet types;
  types.insert(BOOKMARKS);
  EXPECT_EQ(types, registrar.ConfigureDataTypes(types, ModelTypeSet()));
  registrar.ActivateDataType(BOOKMARKS, GROUP_UI,
                             &change_processor_mock,
                             test_user_share_.user_share());
  {
    ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[BOOKMARKS] = GROUP_UI;
    ExpectRoutingInfo(&registrar, expected_routing_info);
  }
  ExpectHasProcessorsForTypes(&registrar, types);

  registrar.DeactivateDataType(BOOKMARKS);
  ExpectRoutingInfo(&registrar, ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(&registrar, ModelTypeSet());

  registrar.OnSyncerShutdownComplete();
  registrar.StopOnUIThread();
}

TEST_F(SyncBackendRegistrarTest, ActivateDeactivateNonUIDataType) {
  BrowserThread db_thread(BrowserThread::DB, &loop_);
  InSequence in_sequence;
  TestingProfile profile;
  SyncBackendRegistrar registrar(ModelTypeSet(), "test", &profile, &loop_);
  StrictMock<ChangeProcessorMock> change_processor_mock;
  EXPECT_CALL(change_processor_mock, StartImpl(&profile));
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, StopImpl());
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(false));

  ModelTypeSet types;
  types.insert(AUTOFILL);
  EXPECT_EQ(types, registrar.ConfigureDataTypes(types, ModelTypeSet()));
  registrar.ActivateDataType(AUTOFILL, GROUP_DB,
                             &change_processor_mock,
                             test_user_share_.user_share());
  {
    ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[AUTOFILL] = GROUP_DB;
    ExpectRoutingInfo(&registrar, expected_routing_info);
  }
  ExpectHasProcessorsForTypes(&registrar, types);

  registrar.DeactivateDataType(AUTOFILL);
  ExpectRoutingInfo(&registrar, ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(&registrar, ModelTypeSet());

  registrar.OnSyncerShutdownComplete();
  registrar.StopOnUIThread();
}

// TODO(akalin): Add tests for non-UI non-passive data types (see
// http://crbug.com/93456).

}  // namespace

}  // namespace browser_sync
