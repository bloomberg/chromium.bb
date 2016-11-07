// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/sync_backend_registrar.h"

#include <memory>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "components/sync/driver/change_processor_mock.h"
#include "components/sync/driver/fake_sync_client.h"
#include "components/sync/driver/glue/browser_thread_model_worker.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/engine/passive_model_worker.h"
#include "components/sync/syncable/test_user_share.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

void TriggerChanges(SyncBackendRegistrar* registrar, ModelType type) {
  registrar->OnChangesApplied(type, 0, nullptr, ImmutableChangeRecordList());
  registrar->OnChangesComplete(type);
}

class RegistrarSyncClient : public FakeSyncClient {
 public:
  RegistrarSyncClient(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner)
      : ui_task_runner_(ui_task_runner),
        db_task_runner_(db_task_runner),
        file_task_runner_(file_task_runner) {}

  scoped_refptr<ModelSafeWorker> CreateModelWorkerForGroup(
      ModelSafeGroup group) override {
    switch (group) {
      case GROUP_UI:
        return new BrowserThreadModelWorker(ui_task_runner_, group);
      case GROUP_DB:
        return new BrowserThreadModelWorker(db_task_runner_, group);
      case GROUP_FILE:
        return new BrowserThreadModelWorker(file_task_runner_, group);
      case GROUP_PASSIVE:
        return new PassiveModelWorker();
      default:
        return nullptr;
    }
  }

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> db_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
};

// Flaky: https://crbug.com/498238
class SyncBackendRegistrarTest : public testing::Test {
 public:
  void TestNonUIDataTypeActivationAsync(ChangeProcessor* processor,
                                        base::WaitableEvent* done) {
    registrar_->ActivateDataType(AUTOFILL, GROUP_DB, processor,
                                 test_user_share_.user_share());
    ExpectRoutingInfo(registrar_.get(), {{AUTOFILL, GROUP_DB}});
    ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet(AUTOFILL));
    TriggerChanges(registrar_.get(), AUTOFILL);
    done->Signal();
  }

 protected:
  SyncBackendRegistrarTest()
      : db_thread_("DBThreadForTest"), file_thread_("FileThreadForTest") {}

  ~SyncBackendRegistrarTest() override {}

  void SetUp() override {
    db_thread_.StartAndWaitForTesting();
    file_thread_.StartAndWaitForTesting();
    test_user_share_.SetUp();
    sync_client_ = base::MakeUnique<RegistrarSyncClient>(
        ui_task_runner(), db_task_runner(), file_task_runner());
    registrar_ = base::MakeUnique<SyncBackendRegistrar>(
        "test", sync_client_.get(), std::unique_ptr<base::Thread>(),
        ui_task_runner(), db_task_runner(), file_task_runner());
  }

  void TearDown() override {
    registrar_->RequestWorkerStopOnUIThread();
    test_user_share_.TearDown();
    {
      std::unique_ptr<base::Thread> released_sync_thread =
          registrar_->ReleaseSyncThread();
      released_sync_thread->task_runner()->DeleteSoon(FROM_HERE,
                                                      registrar_.release());
      // |released_sync_thread| is joined at the end of this scope.
    }
    base::RunLoop().RunUntilIdle();
  }

  void ExpectRoutingInfo(SyncBackendRegistrar* registrar,
                         const ModelSafeRoutingInfo& expected_routing_info) {
    ModelSafeRoutingInfo actual_routing_info;
    registrar->GetModelSafeRoutingInfo(&actual_routing_info);
    EXPECT_EQ(expected_routing_info, actual_routing_info);
  }

  void ExpectHasProcessorsForTypes(const SyncBackendRegistrar& registrar,
                                   ModelTypeSet types) {
    for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
      ModelType model_type = ModelTypeFromInt(i);
      EXPECT_EQ(types.Has(model_type),
                registrar_->IsTypeActivatedForTest(model_type));
    }
  }

  size_t GetWorkersSize() {
    std::vector<scoped_refptr<ModelSafeWorker>> workers;
    registrar_->GetWorkers(&workers);
    return workers.size();
  }

  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner() {
    return message_loop_.task_runner();
  }

  const scoped_refptr<base::SingleThreadTaskRunner> db_task_runner() {
    return db_thread_.task_runner();
  }

  const scoped_refptr<base::SingleThreadTaskRunner> file_task_runner() {
    return db_thread_.task_runner();
  }

  base::MessageLoop message_loop_;
  base::Thread db_thread_;
  base::Thread file_thread_;

  TestUserShare test_user_share_;
  std::unique_ptr<RegistrarSyncClient> sync_client_;
  std::unique_ptr<SyncBackendRegistrar> registrar_;
};

TEST_F(SyncBackendRegistrarTest, ConstructorEmpty) {
  registrar_->SetInitialTypes(ModelTypeSet());
  EXPECT_FALSE(registrar_->IsNigoriEnabled());
  EXPECT_EQ(4u, GetWorkersSize());
  ExpectRoutingInfo(registrar_.get(), ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());
}

TEST_F(SyncBackendRegistrarTest, ConstructorNonEmpty) {
  registrar_->RegisterNonBlockingType(BOOKMARKS);
  registrar_->SetInitialTypes(ModelTypeSet(BOOKMARKS, NIGORI, PASSWORDS));
  EXPECT_TRUE(registrar_->IsNigoriEnabled());
  EXPECT_EQ(4u, GetWorkersSize());
  EXPECT_EQ(ModelTypeSet(NIGORI), registrar_->GetLastConfiguredTypes());
  // Bookmarks dropped because it is nonblocking.
  // Passwords dropped because of no password store.
  ExpectRoutingInfo(registrar_.get(), {{NIGORI, GROUP_PASSIVE}});
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());
}

TEST_F(SyncBackendRegistrarTest, ConstructorNonEmptyReversedInitialization) {
  // The blocking types get to set initial types before NonBlocking types here.
  registrar_->SetInitialTypes(ModelTypeSet(BOOKMARKS, NIGORI, PASSWORDS));
  registrar_->RegisterNonBlockingType(BOOKMARKS);
  EXPECT_TRUE(registrar_->IsNigoriEnabled());
  EXPECT_EQ(4u, GetWorkersSize());
  EXPECT_EQ(ModelTypeSet(NIGORI), registrar_->GetLastConfiguredTypes());
  // Bookmarks dropped because it is nonblocking.
  // Passwords dropped because of no password store.
  ExpectRoutingInfo(registrar_.get(), {{NIGORI, GROUP_PASSIVE}});
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());
}

TEST_F(SyncBackendRegistrarTest, ConfigureDataTypes) {
  registrar_->RegisterNonBlockingType(BOOKMARKS);
  registrar_->SetInitialTypes(ModelTypeSet());

  // Add.
  const ModelTypeSet types1(BOOKMARKS, NIGORI, AUTOFILL);
  EXPECT_EQ(types1, registrar_->ConfigureDataTypes(types1, ModelTypeSet()));
  ExpectRoutingInfo(registrar_.get(), {{BOOKMARKS, GROUP_NON_BLOCKING},
                                       {NIGORI, GROUP_PASSIVE},
                                       {AUTOFILL, GROUP_PASSIVE}});
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());
  EXPECT_EQ(types1, registrar_->GetLastConfiguredTypes());

  // Add and remove.
  const ModelTypeSet types2(PREFERENCES, THEMES);
  EXPECT_EQ(types2, registrar_->ConfigureDataTypes(types2, types1));

  ExpectRoutingInfo(registrar_.get(),
                    {{PREFERENCES, GROUP_PASSIVE}, {THEMES, GROUP_PASSIVE}});
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());
  EXPECT_EQ(types2, registrar_->GetLastConfiguredTypes());

  // Remove.
  EXPECT_TRUE(registrar_->ConfigureDataTypes(ModelTypeSet(), types2).Empty());
  ExpectRoutingInfo(registrar_.get(), ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());
  EXPECT_EQ(ModelTypeSet(), registrar_->GetLastConfiguredTypes());
}

TEST_F(SyncBackendRegistrarTest, ActivateDeactivateUIDataType) {
  InSequence in_sequence;
  registrar_->SetInitialTypes(ModelTypeSet());

  // Should do nothing.
  TriggerChanges(registrar_.get(), BOOKMARKS);

  StrictMock<ChangeProcessorMock> change_processor_mock;
  EXPECT_CALL(change_processor_mock, StartImpl());
  EXPECT_CALL(change_processor_mock, IsRunning()).WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, ApplyChangesFromSyncModel(nullptr, _, _));
  EXPECT_CALL(change_processor_mock, IsRunning()).WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, CommitChangesFromSyncModel());
  EXPECT_CALL(change_processor_mock, IsRunning()).WillRepeatedly(Return(false));

  const ModelTypeSet types(BOOKMARKS);
  EXPECT_EQ(types, registrar_->ConfigureDataTypes(types, ModelTypeSet()));
  registrar_->ActivateDataType(BOOKMARKS, GROUP_UI, &change_processor_mock,
                               test_user_share_.user_share());
  ExpectRoutingInfo(registrar_.get(), {{BOOKMARKS, GROUP_UI}});
  ExpectHasProcessorsForTypes(*registrar_, types);

  TriggerChanges(registrar_.get(), BOOKMARKS);

  registrar_->DeactivateDataType(BOOKMARKS);
  ExpectRoutingInfo(registrar_.get(), ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());

  // Should do nothing.
  TriggerChanges(registrar_.get(), BOOKMARKS);
}

TEST_F(SyncBackendRegistrarTest, ActivateDeactivateNonUIDataType) {
  InSequence in_sequence;
  registrar_->SetInitialTypes(ModelTypeSet());

  // Should do nothing.
  TriggerChanges(registrar_.get(), AUTOFILL);

  StrictMock<ChangeProcessorMock> change_processor_mock;
  EXPECT_CALL(change_processor_mock, StartImpl());
  EXPECT_CALL(change_processor_mock, IsRunning()).WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, ApplyChangesFromSyncModel(nullptr, _, _));
  EXPECT_CALL(change_processor_mock, IsRunning()).WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, CommitChangesFromSyncModel());
  EXPECT_CALL(change_processor_mock, IsRunning()).WillRepeatedly(Return(false));

  const ModelTypeSet types(AUTOFILL);
  EXPECT_EQ(types, registrar_->ConfigureDataTypes(types, ModelTypeSet()));

  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  db_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendRegistrarTest::TestNonUIDataTypeActivationAsync,
                 base::Unretained(this), &change_processor_mock, &done));
  done.Wait();

  registrar_->DeactivateDataType(AUTOFILL);
  ExpectRoutingInfo(registrar_.get(), ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());

  // Should do nothing.
  TriggerChanges(registrar_.get(), AUTOFILL);
}

// Tests that registration and configuration of non-blocking data types is
// handled correctly in SyncBackendRegistrar.
TEST_F(SyncBackendRegistrarTest, ConfigureNonBlockingDataType) {
  registrar_->RegisterNonBlockingType(AUTOFILL);
  registrar_->RegisterNonBlockingType(BOOKMARKS);

  ExpectRoutingInfo(registrar_.get(), ModelSafeRoutingInfo());
  // Simulate that initial sync was already done for AUTOFILL.
  registrar_->AddRestoredNonBlockingType(AUTOFILL);
  // It should be added to routing info and set of configured types.
  EXPECT_EQ(ModelTypeSet(AUTOFILL), registrar_->GetLastConfiguredTypes());
  ExpectRoutingInfo(registrar_.get(), {{AUTOFILL, GROUP_NON_BLOCKING}});

  // Configure two non-blocking types. Initial sync wasn't done for BOOKMARKS so
  // it should be included in types to be downloaded.
  ModelTypeSet types_to_add(AUTOFILL, BOOKMARKS);
  ModelTypeSet newly_added_types =
      registrar_->ConfigureDataTypes(types_to_add, ModelTypeSet());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), newly_added_types);
  EXPECT_EQ(types_to_add, registrar_->GetLastConfiguredTypes());
  ExpectRoutingInfo(registrar_.get(), {{AUTOFILL, GROUP_NON_BLOCKING},
                                       {BOOKMARKS, GROUP_NON_BLOCKING}});
}

}  // namespace

}  // namespace syncer
