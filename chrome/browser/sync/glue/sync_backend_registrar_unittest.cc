// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_backend_registrar.h"

#include "chrome/browser/sync/glue/change_processor_mock.h"
#include "chrome/browser/sync/glue/ui_model_worker.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/test/engine/test_user_share.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
using content::BrowserThread;
using syncable::FIRST_REAL_MODEL_TYPE;
using syncable::AUTOFILL;
using syncable::BOOKMARKS;
using syncable::PREFERENCES;
using syncable::THEMES;
using syncable::NIGORI;
using syncable::PASSWORDS;
using syncable::MODEL_TYPE_COUNT;
using syncable::ModelEnumSet;
using syncable::ModelType;
using syncable::ModelTypeFromInt;

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

  void ExpectHasProcessorsForTypes(const SyncBackendRegistrar& registrar,
                                   ModelEnumSet types) {
    for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
      ModelType model_type = ModelTypeFromInt(i);
      EXPECT_EQ(types.Has(model_type),
                registrar.IsTypeActivatedForTest(model_type));
    }
  }

  MessageLoop loop_;
  TestUserShare test_user_share_;

 private:
  content::TestBrowserThread ui_thread_;
};

TEST_F(SyncBackendRegistrarTest, ConstructorEmpty) {
  TestingProfile profile;
  SyncBackendRegistrar registrar(ModelEnumSet(), "test", &profile, &loop_);
  EXPECT_FALSE(registrar.IsNigoriEnabled());
  {
    std::vector<ModelSafeWorker*> workers;
    registrar.GetWorkers(&workers);
    EXPECT_EQ(4u, workers.size());
  }
  ExpectRoutingInfo(&registrar, ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(registrar, ModelEnumSet());
  registrar.OnSyncerShutdownComplete();
  registrar.StopOnUIThread();
}

TEST_F(SyncBackendRegistrarTest, ConstructorNonEmpty) {
  TestingProfile profile;
  const ModelEnumSet initial_types(BOOKMARKS, NIGORI, PASSWORDS);
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
  ExpectHasProcessorsForTypes(registrar, ModelEnumSet());
  registrar.OnSyncerShutdownComplete();
  registrar.StopOnUIThread();
}

TEST_F(SyncBackendRegistrarTest, ConfigureDataTypes) {
  TestingProfile profile;
  SyncBackendRegistrar registrar(ModelEnumSet(), "test", &profile, &loop_);

  // Add.
  const ModelEnumSet types1(BOOKMARKS, NIGORI, AUTOFILL);
  EXPECT_TRUE(
      registrar.ConfigureDataTypes(types1, ModelEnumSet()).Equals(types1));
  {
    ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[BOOKMARKS] = GROUP_PASSIVE;
    expected_routing_info[NIGORI] = GROUP_PASSIVE;
    expected_routing_info[AUTOFILL] = GROUP_PASSIVE;
    ExpectRoutingInfo(&registrar, expected_routing_info);
  }
  ExpectHasProcessorsForTypes(registrar, ModelEnumSet());

  // Add and remove.
  const ModelEnumSet types2(PREFERENCES, THEMES);
  EXPECT_TRUE(registrar.ConfigureDataTypes(types2, types1).Equals(types2));
  {
    ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[PREFERENCES] = GROUP_PASSIVE;
    expected_routing_info[THEMES] = GROUP_PASSIVE;
    ExpectRoutingInfo(&registrar, expected_routing_info);
  }
  ExpectHasProcessorsForTypes(registrar, ModelEnumSet());

  // Remove.
  EXPECT_TRUE(registrar.ConfigureDataTypes(ModelEnumSet(), types2).Empty());
  ExpectRoutingInfo(&registrar, ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(registrar, ModelEnumSet());

  registrar.OnSyncerShutdownComplete();
  registrar.StopOnUIThread();
}

void TriggerChanges(SyncBackendRegistrar* registrar, ModelType type) {
  registrar->OnChangesApplied(type, NULL,
                              sync_api::ImmutableChangeRecordList());
  registrar->OnChangesComplete(type);
}

TEST_F(SyncBackendRegistrarTest, ActivateDeactivateUIDataType) {
  InSequence in_sequence;
  TestingProfile profile;
  SyncBackendRegistrar registrar(ModelEnumSet(), "test", &profile, &loop_);

  // Should do nothing.
  TriggerChanges(&registrar, BOOKMARKS);

  StrictMock<ChangeProcessorMock> change_processor_mock;
  EXPECT_CALL(change_processor_mock, StartImpl(&profile));
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, ApplyChangesFromSyncModel(NULL, _));
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, CommitChangesFromSyncModel());
  EXPECT_CALL(change_processor_mock, StopImpl());
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(false));

  const ModelEnumSet types(BOOKMARKS);
  EXPECT_TRUE(
      registrar.ConfigureDataTypes(types, ModelEnumSet()).Equals(types));
  registrar.ActivateDataType(BOOKMARKS, GROUP_UI,
                             &change_processor_mock,
                             test_user_share_.user_share());
  {
    ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[BOOKMARKS] = GROUP_UI;
    ExpectRoutingInfo(&registrar, expected_routing_info);
  }
  ExpectHasProcessorsForTypes(registrar, types);

  TriggerChanges(&registrar, BOOKMARKS);

  registrar.DeactivateDataType(BOOKMARKS);
  ExpectRoutingInfo(&registrar, ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(registrar, ModelEnumSet());

  // Should do nothing.
  TriggerChanges(&registrar, BOOKMARKS);

  registrar.OnSyncerShutdownComplete();
  registrar.StopOnUIThread();
}

TEST_F(SyncBackendRegistrarTest, ActivateDeactivateNonUIDataType) {
  content::TestBrowserThread db_thread(BrowserThread::DB, &loop_);
  InSequence in_sequence;
  TestingProfile profile;
  SyncBackendRegistrar registrar(ModelEnumSet(), "test", &profile, &loop_);

  // Should do nothing.
  TriggerChanges(&registrar, AUTOFILL);

  StrictMock<ChangeProcessorMock> change_processor_mock;
  EXPECT_CALL(change_processor_mock, StartImpl(&profile));
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, ApplyChangesFromSyncModel(NULL, _));
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, CommitChangesFromSyncModel());
  EXPECT_CALL(change_processor_mock, StopImpl());
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(false));

  const ModelEnumSet types(AUTOFILL);
  EXPECT_TRUE(
      registrar.ConfigureDataTypes(types, ModelEnumSet()).Equals(types));
  registrar.ActivateDataType(AUTOFILL, GROUP_DB,
                             &change_processor_mock,
                             test_user_share_.user_share());
  {
    ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[AUTOFILL] = GROUP_DB;
    ExpectRoutingInfo(&registrar, expected_routing_info);
  }
  ExpectHasProcessorsForTypes(registrar, types);

  TriggerChanges(&registrar, AUTOFILL);

  registrar.DeactivateDataType(AUTOFILL);
  ExpectRoutingInfo(&registrar, ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(registrar, ModelEnumSet());

  // Should do nothing.
  TriggerChanges(&registrar, AUTOFILL);

  registrar.OnSyncerShutdownComplete();
  registrar.StopOnUIThread();
}

}  // namespace

}  // namespace browser_sync
