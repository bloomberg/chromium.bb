// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_backend_registrar.h"

#include "chrome/browser/sync/glue/ui_model_worker.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_driver/change_processor_mock.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
using content::BrowserThread;
using syncer::FIRST_REAL_MODEL_TYPE;
using syncer::AUTOFILL;
using syncer::BOOKMARKS;
using syncer::PREFERENCES;
using syncer::THEMES;
using syncer::NIGORI;
using syncer::PASSWORDS;
using syncer::MODEL_TYPE_COUNT;
using syncer::ModelTypeSet;
using syncer::ModelType;
using syncer::ModelTypeFromInt;

void TriggerChanges(SyncBackendRegistrar* registrar, ModelType type) {
  registrar->OnChangesApplied(type, 0, NULL,
                              syncer::ImmutableChangeRecordList());
  registrar->OnChangesComplete(type);
}

class SyncBackendRegistrarTest : public testing::Test {
 public:
  void TestNonUIDataTypeActivationAsync(sync_driver::ChangeProcessor* processor,
                                        base::WaitableEvent* done) {
    registrar_->ActivateDataType(AUTOFILL,
                                 syncer::GROUP_DB,
                                 processor,
                                 test_user_share_.user_share());
    syncer::ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[AUTOFILL] = syncer::GROUP_DB;
    ExpectRoutingInfo(registrar_.get(), expected_routing_info);
    ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet(AUTOFILL));
    TriggerChanges(registrar_.get(), AUTOFILL);
    done->Signal();
  }

 protected:
  SyncBackendRegistrarTest()
      : sync_thread_(NULL),
        thread_bundle_(content::TestBrowserThreadBundle::REAL_DB_THREAD |
                       content::TestBrowserThreadBundle::REAL_FILE_THREAD |
                       content::TestBrowserThreadBundle::REAL_IO_THREAD) {}

  virtual ~SyncBackendRegistrarTest() {}

  virtual void SetUp() {
    test_user_share_.SetUp();
    registrar_.reset(new SyncBackendRegistrar("test", &profile_,
                                              scoped_ptr<base::Thread>()));
    sync_thread_ = registrar_->sync_thread();
  }

  virtual void TearDown() {
    registrar_->RequestWorkerStopOnUIThread();
    test_user_share_.TearDown();
    sync_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&SyncBackendRegistrar::Shutdown,
                   base::Unretained(registrar_.release())));
    sync_thread_->message_loop()->RunUntilIdle();
  }

  void ExpectRoutingInfo(
      SyncBackendRegistrar* registrar,
      const syncer::ModelSafeRoutingInfo& expected_routing_info) {
    syncer::ModelSafeRoutingInfo routing_info;
    registrar->GetModelSafeRoutingInfo(&routing_info);
    EXPECT_EQ(expected_routing_info, routing_info);
  }

  void ExpectHasProcessorsForTypes(const SyncBackendRegistrar& registrar,
                                   ModelTypeSet types) {
    for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
      ModelType model_type = ModelTypeFromInt(i);
      EXPECT_EQ(types.Has(model_type),
                registrar_->IsTypeActivatedForTest(model_type));
    }
  }

  syncer::TestUserShare test_user_share_;
  TestingProfile profile_;
  scoped_ptr<SyncBackendRegistrar> registrar_;

  base::Thread* sync_thread_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(SyncBackendRegistrarTest, ConstructorEmpty) {
  registrar_->SetInitialTypes(ModelTypeSet());
  EXPECT_FALSE(registrar_->IsNigoriEnabled());
  {
    std::vector<scoped_refptr<syncer::ModelSafeWorker> > workers;
    registrar_->GetWorkers(&workers);
    EXPECT_EQ(4u, workers.size());
  }
  ExpectRoutingInfo(registrar_.get(), syncer::ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());
}

TEST_F(SyncBackendRegistrarTest, ConstructorNonEmpty) {
  const ModelTypeSet initial_types(BOOKMARKS, NIGORI, PASSWORDS);
  registrar_->SetInitialTypes(initial_types);
  EXPECT_TRUE(registrar_->IsNigoriEnabled());
  {
    std::vector<scoped_refptr<syncer::ModelSafeWorker> > workers;
    registrar_->GetWorkers(&workers);
    EXPECT_EQ(4u, workers.size());
  }
  {
    syncer::ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[BOOKMARKS] = syncer::GROUP_PASSIVE;
    expected_routing_info[NIGORI] = syncer::GROUP_PASSIVE;
    // Passwords dropped because of no password store.
    ExpectRoutingInfo(registrar_.get(), expected_routing_info);
  }
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());
}

TEST_F(SyncBackendRegistrarTest, ConfigureDataTypes) {
  registrar_->SetInitialTypes(ModelTypeSet());

  // Add.
  const ModelTypeSet types1(BOOKMARKS, NIGORI, AUTOFILL);
  EXPECT_TRUE(
      registrar_->ConfigureDataTypes(types1, ModelTypeSet()).Equals(types1));
  {
    syncer::ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[BOOKMARKS] = syncer::GROUP_PASSIVE;
    expected_routing_info[NIGORI] = syncer::GROUP_PASSIVE;
    expected_routing_info[AUTOFILL] = syncer::GROUP_PASSIVE;
    ExpectRoutingInfo(registrar_.get(), expected_routing_info);
  }
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());
  EXPECT_TRUE(types1.Equals(registrar_->GetLastConfiguredTypes()));

  // Add and remove.
  const ModelTypeSet types2(PREFERENCES, THEMES);
  EXPECT_TRUE(registrar_->ConfigureDataTypes(types2, types1).Equals(types2));
  {
    syncer::ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[PREFERENCES] = syncer::GROUP_PASSIVE;
    expected_routing_info[THEMES] = syncer::GROUP_PASSIVE;
    ExpectRoutingInfo(registrar_.get(), expected_routing_info);
  }
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());
  EXPECT_TRUE(types2.Equals(registrar_->GetLastConfiguredTypes()));

  // Remove.
  EXPECT_TRUE(registrar_->ConfigureDataTypes(ModelTypeSet(), types2).Empty());
  ExpectRoutingInfo(registrar_.get(), syncer::ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());
  EXPECT_TRUE(ModelTypeSet().Equals(registrar_->GetLastConfiguredTypes()));
}

TEST_F(SyncBackendRegistrarTest, ActivateDeactivateUIDataType) {
  InSequence in_sequence;
  registrar_->SetInitialTypes(ModelTypeSet());

  // Should do nothing.
  TriggerChanges(registrar_.get(), BOOKMARKS);

  StrictMock<sync_driver::ChangeProcessorMock> change_processor_mock;
  EXPECT_CALL(change_processor_mock, StartImpl());
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, ApplyChangesFromSyncModel(NULL, _, _));
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, CommitChangesFromSyncModel());
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(false));

  const ModelTypeSet types(BOOKMARKS);
  EXPECT_TRUE(
      registrar_->ConfigureDataTypes(types, ModelTypeSet()).Equals(types));
  registrar_->ActivateDataType(BOOKMARKS, syncer::GROUP_UI,
                             &change_processor_mock,
                             test_user_share_.user_share());
  {
    syncer::ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[BOOKMARKS] = syncer::GROUP_UI;
    ExpectRoutingInfo(registrar_.get(), expected_routing_info);
  }
  ExpectHasProcessorsForTypes(*registrar_, types);

  TriggerChanges(registrar_.get(), BOOKMARKS);

  registrar_->DeactivateDataType(BOOKMARKS);
  ExpectRoutingInfo(registrar_.get(), syncer::ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());

  // Should do nothing.
  TriggerChanges(registrar_.get(), BOOKMARKS);
}

TEST_F(SyncBackendRegistrarTest, ActivateDeactivateNonUIDataType) {
  InSequence in_sequence;
  registrar_->SetInitialTypes(ModelTypeSet());

  // Should do nothing.
  TriggerChanges(registrar_.get(), AUTOFILL);

  StrictMock<sync_driver::ChangeProcessorMock> change_processor_mock;
  EXPECT_CALL(change_processor_mock, StartImpl());
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, ApplyChangesFromSyncModel(NULL, _, _));
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, CommitChangesFromSyncModel());
  EXPECT_CALL(change_processor_mock, IsRunning())
      .WillRepeatedly(Return(false));

  const ModelTypeSet types(AUTOFILL);
  EXPECT_TRUE(
      registrar_->ConfigureDataTypes(types, ModelTypeSet()).Equals(types));

  base::WaitableEvent done(false, false);
  BrowserThread::PostTask(
      BrowserThread::DB,
      FROM_HERE,
      base::Bind(&SyncBackendRegistrarTest::TestNonUIDataTypeActivationAsync,
                 base::Unretained(this),
                 &change_processor_mock,
                 &done));
  done.Wait();

  registrar_->DeactivateDataType(AUTOFILL);
  ExpectRoutingInfo(registrar_.get(), syncer::ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());

  // Should do nothing.
  TriggerChanges(registrar_.get(), AUTOFILL);
}

}  // namespace

}  // namespace browser_sync
