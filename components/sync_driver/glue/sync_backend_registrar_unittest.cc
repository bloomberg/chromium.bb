// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/glue/sync_backend_registrar.h"

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "components/sync_driver/change_processor_mock.h"
#include "components/sync_driver/fake_sync_client.h"
#include "components/sync_driver/glue/browser_thread_model_worker.h"
#include "components/sync_driver/sync_api_component_factory_mock.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/passive_model_worker.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
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

class RegistrarSyncClient : public sync_driver::FakeSyncClient {
 public:
  RegistrarSyncClient(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner)
      : ui_task_runner_(ui_task_runner),
        db_task_runner_(db_task_runner),
        file_task_runner_(file_task_runner) {}

  scoped_refptr<syncer::ModelSafeWorker> CreateModelWorkerForGroup(
      syncer::ModelSafeGroup group,
      syncer::WorkerLoopDestructionObserver* observer) override {
    switch (group) {
      case syncer::GROUP_UI:
        return new BrowserThreadModelWorker(ui_task_runner_, group, observer);
      case syncer::GROUP_DB:
        return new BrowserThreadModelWorker(db_task_runner_, group, observer);
      case syncer::GROUP_FILE:
        return new BrowserThreadModelWorker(file_task_runner_, group, observer);
      case syncer::GROUP_PASSIVE:
        return new syncer::PassiveModelWorker(observer);
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
      : db_thread_("DBThreadForTest"),
        file_thread_("FileThreadForTest"),
        sync_thread_(NULL) {}

  ~SyncBackendRegistrarTest() override {}

  void SetUp() override {
    db_thread_.StartAndWaitForTesting();
    file_thread_.StartAndWaitForTesting();
    test_user_share_.SetUp();
    sync_client_.reset(new RegistrarSyncClient(
        ui_task_runner(), db_task_runner(), file_task_runner()));
    registrar_.reset(new SyncBackendRegistrar(
        "test", sync_client_.get(), std::unique_ptr<base::Thread>(),
        ui_task_runner(), db_task_runner(), file_task_runner()));
    sync_thread_ = registrar_->sync_thread();
  }

  void TearDown() override {
    registrar_->RequestWorkerStopOnUIThread();
    test_user_share_.TearDown();
    sync_thread_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&SyncBackendRegistrar::Shutdown,
                              base::Unretained(registrar_.release())));
    sync_thread_->WaitUntilThreadStarted();
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

  syncer::TestUserShare test_user_share_;
  std::unique_ptr<RegistrarSyncClient> sync_client_;
  std::unique_ptr<SyncBackendRegistrar> registrar_;

  base::Thread* sync_thread_;
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
  registrar_->RegisterNonBlockingType(BOOKMARKS);
  registrar_->SetInitialTypes(initial_types);
  EXPECT_TRUE(registrar_->IsNigoriEnabled());
  {
    std::vector<scoped_refptr<syncer::ModelSafeWorker> > workers;
    registrar_->GetWorkers(&workers);
    EXPECT_EQ(4u, workers.size());
  }
  {
    syncer::ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[BOOKMARKS] = syncer::GROUP_NON_BLOCKING;
    expected_routing_info[NIGORI] = syncer::GROUP_PASSIVE;
    // Passwords dropped because of no password store.
    ExpectRoutingInfo(registrar_.get(), expected_routing_info);
  }
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());
}

TEST_F(SyncBackendRegistrarTest, ConfigureDataTypes) {
  registrar_->RegisterNonBlockingType(BOOKMARKS);
  registrar_->SetInitialTypes(ModelTypeSet());

  // Add.
  const ModelTypeSet types1(BOOKMARKS, NIGORI, AUTOFILL);
  EXPECT_TRUE(
      registrar_->ConfigureDataTypes(types1, ModelTypeSet()).Equals(types1));
  {
    syncer::ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[BOOKMARKS] = syncer::GROUP_NON_BLOCKING;
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
  db_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendRegistrarTest::TestNonUIDataTypeActivationAsync,
                 base::Unretained(this), &change_processor_mock, &done));
  done.Wait();

  registrar_->DeactivateDataType(AUTOFILL);
  ExpectRoutingInfo(registrar_.get(), syncer::ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(*registrar_, ModelTypeSet());

  // Should do nothing.
  TriggerChanges(registrar_.get(), AUTOFILL);
}

class SyncBackendRegistrarShutdownTest : public testing::Test {
 public:
  void BlockDBThread() {
    EXPECT_FALSE(db_thread_lock_.Try());

    db_thread_blocked_.Signal();
    base::AutoLock l(db_thread_lock_);
  }

 protected:
  friend class TestRegistrar;

  SyncBackendRegistrarShutdownTest()
      : db_thread_("DBThreadForTest"),
        file_thread_("FileThreadForTest"),
        db_thread_blocked_(false, false) {
    quit_closure_ = run_loop_.QuitClosure();
  }

  ~SyncBackendRegistrarShutdownTest() override {}

  void SetUp() override {
    db_thread_.StartAndWaitForTesting();
    file_thread_.StartAndWaitForTesting();
    sync_client_.reset(new RegistrarSyncClient(
        ui_task_runner(), db_task_runner(), file_task_runner()));
  }

  void PostQuitOnUIMessageLoop() {
    ui_task_runner()->PostTask(FROM_HERE, quit_closure_);
  }

  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner() {
    return message_loop_.task_runner();
  }

  const scoped_refptr<base::SingleThreadTaskRunner> db_task_runner() {
    return db_thread_.task_runner();
  }

  const scoped_refptr<base::SingleThreadTaskRunner> file_task_runner() {
    return file_thread_.task_runner();
  }

  base::MessageLoop message_loop_;
  base::Thread db_thread_;
  base::Thread file_thread_;

  std::unique_ptr<RegistrarSyncClient> sync_client_;
  base::WaitableEvent db_thread_blocked_;

  base::Lock db_thread_lock_;
  base::RunLoop run_loop_;
  base::Closure quit_closure_;
};

// Wrap SyncBackendRegistrar so that we can monitor its lifetime.
class TestRegistrar : public SyncBackendRegistrar {
 public:
  explicit TestRegistrar(
      sync_driver::SyncClient* sync_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& file_thread,
      SyncBackendRegistrarShutdownTest* test)
      : SyncBackendRegistrar("test",
                             sync_client,
                             std::unique_ptr<base::Thread>(),
                             ui_thread,
                             db_thread,
                             file_thread),
        test_(test) {}

  ~TestRegistrar() override { test_->PostQuitOnUIMessageLoop(); }

 private:
  SyncBackendRegistrarShutdownTest* test_;
};

TEST_F(SyncBackendRegistrarShutdownTest, BlockingShutdown) {
  // Take ownership of |db_thread_lock_| so that the DB thread can't acquire it.
  db_thread_lock_.Acquire();

  // This will block the DB thread by waiting on |db_thread_lock_|.
  db_task_runner()->PostTask(
      FROM_HERE, base::Bind(&SyncBackendRegistrarShutdownTest::BlockDBThread,
                            base::Unretained(this)));

  std::unique_ptr<TestRegistrar> registrar(
      new TestRegistrar(sync_client_.get(), ui_task_runner(), db_task_runner(),
                        file_task_runner(), this));
  base::Thread* sync_thread = registrar->sync_thread();

  // Stop here until the DB thread gets a chance to run and block on the lock.
  // Please note that since the task above didn't finish, the task to
  // initialize the worker on the DB thread hasn't had a chance to run yet too.
  // Which means ModelSafeWorker::SetWorkingLoopToCurrent hasn't been called
  // for the DB worker.
  db_thread_blocked_.Wait();

  registrar->SetInitialTypes(ModelTypeSet());

  // Start the shutdown.
  registrar->RequestWorkerStopOnUIThread();

  sync_thread->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SyncBackendRegistrar::Shutdown,
                            base::Unretained(registrar.release())));

  // Make sure the thread starts running.
  sync_thread->WaitUntilThreadStarted();

  // The test verifies that the sync thread doesn't block because
  // of the blocked DB thread and can finish the shutdown.
  sync_thread->message_loop()->RunUntilIdle();

  db_thread_lock_.Release();

  // Run the main thread loop until all workers have been removed and the
  // registrar destroyed.
  run_loop_.Run();
}

}  // namespace

}  // namespace browser_sync
