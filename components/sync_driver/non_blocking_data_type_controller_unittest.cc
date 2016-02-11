// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/non_blocking_data_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "components/sync_driver/backend_data_type_configurer.h"
#include "components/sync_driver/fake_sync_client.h"
#include "sync/engine/commit_queue.h"
#include "sync/internal_api/public/activation_context.h"
#include "sync/internal_api/public/shared_model_type_processor.h"
#include "sync/internal_api/public/test/fake_model_type_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_driver_v2 {

namespace {

// Test controller derived from NonBlockingDataTypeController.
class TestController : public NonBlockingDataTypeController {
 public:
  TestController(const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
                 const base::Closure& error_callback,
                 syncer::ModelType model_type,
                 sync_driver::SyncClient* sync_client)
      : NonBlockingDataTypeController(ui_thread, error_callback, sync_client),
        model_type_(model_type) {}

  // TODO(stanisc): This will likely have to change. It should be controller's
  // job to locate the service via SyncClient, create an instance of
  // SharedModelTypeProcessor, pass it to the service to own, and to store the
  // weak pointer to the type processor. For now this continues using the
  // earlier design where the controller and it's task runner are initialized
  // from outside.
  // Note that for the test purposes a nullptr |model_task_runner| indicates a
  // special case of running on UI thread (see RunOnModelThread below).
  void Initialize(const scoped_refptr<base::TaskRunner>& model_task_runner,
                  const base::WeakPtr<syncer_v2::SharedModelTypeProcessor>&
                      type_processor) {
    model_task_runner_ = model_task_runner;
    type_processor_ = type_processor;
  }

  syncer::ModelType type() const override { return model_type_; }

  base::WeakPtr<syncer_v2::SharedModelTypeProcessor> type_processor()
      const override {
    return type_processor_;
  }

  bool RunOnModelThread(const tracked_objects::Location& from_here,
                        const base::Closure& task) override {
    if (model_task_runner_) {
      return model_task_runner_->PostTask(from_here, task);
    } else {
      // Special case for model running on the UI thread.
      task.Run();
      return true;
    }
  }

 private:
  ~TestController() override {}

  syncer::ModelType model_type_;
  base::WeakPtr<syncer_v2::SharedModelTypeProcessor> type_processor_;
  scoped_refptr<base::TaskRunner> model_task_runner_;
};

// A no-op instance of CommitQueue.
class NullCommitQueue : public syncer_v2::CommitQueue {
 public:
  NullCommitQueue() {}
  ~NullCommitQueue() override {}

  void EnqueueForCommit(const syncer_v2::CommitRequestDataList& list) override {
    NOTREACHED() << "Not implemented.";
  }
};

// A class that pretends to be the sync backend.
class MockSyncBackend {
 public:
  void Connect(syncer::ModelType type,
               scoped_ptr<syncer_v2::ActivationContext> activation_context) {
    enabled_types_.Put(type);
    activation_context->type_processor->ConnectSync(
        make_scoped_ptr(new NullCommitQueue()));
  }

  void Disconnect(syncer::ModelType type) {
    DCHECK(enabled_types_.Has(type));
    enabled_types_.Remove(type);
  }

 private:
  syncer::ModelTypeSet enabled_types_;
};

// Fake implementation of BackendDataTypeConfigurer that pretends to be Sync
// backend.
class MockBackendDataTypeConfigurer
    : public sync_driver::BackendDataTypeConfigurer {
 public:
  MockBackendDataTypeConfigurer(
      MockSyncBackend* backend,
      const scoped_refptr<base::TaskRunner>& sync_task_runner)
      : backend_(backend), sync_task_runner_(sync_task_runner) {}
  ~MockBackendDataTypeConfigurer() override {}

  syncer::ModelTypeSet ConfigureDataTypes(
      syncer::ConfigureReason reason,
      const DataTypeConfigStateMap& config_state_map,
      const base::Callback<void(syncer::ModelTypeSet, syncer::ModelTypeSet)>&
          ready_task,
      const base::Callback<void()>& retry_callback) override {
    NOTREACHED() << "Not implemented.";
    return syncer::ModelTypeSet();
  }

  void ActivateDirectoryDataType(
      syncer::ModelType type,
      syncer::ModelSafeGroup group,
      sync_driver::ChangeProcessor* change_processor) override {
    NOTREACHED() << "Not implemented.";
  }

  void DeactivateDirectoryDataType(syncer::ModelType type) override {
    NOTREACHED() << "Not implemented.";
  }

  void ActivateNonBlockingDataType(
      syncer::ModelType type,
      scoped_ptr<syncer_v2::ActivationContext> activation_context) override {
    // Post on Sync thread just like the real implementation does.
    sync_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MockSyncBackend::Connect, base::Unretained(backend_), type,
                   base::Passed(std::move(activation_context))));
  }

  void DeactivateNonBlockingDataType(syncer::ModelType type) override {
    sync_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&MockSyncBackend::Disconnect,
                                           base::Unretained(backend_), type));
  }

 private:
  MockSyncBackend* backend_;
  scoped_refptr<base::TaskRunner> sync_task_runner_;
};

}  // namespace

class NonBlockingDataTypeControllerTest : public testing::Test,
                                          public sync_driver::FakeSyncClient {
 public:
  NonBlockingDataTypeControllerTest()
      : auto_run_tasks_(true),
        load_models_callback_called_(false),
        association_callback_called_(false),
        model_thread_("modelthread"),
        sync_thread_runner_(new base::TestSimpleTaskRunner()),
        configurer_(&backend_, sync_thread_runner_) {}

  ~NonBlockingDataTypeControllerTest() override {}

  void SetUp() override {
    controller_ = new TestController(ui_loop_.task_runner(), base::Closure(),
                                     syncer::DICTIONARY, this);
  }

  void TearDown() override {
    ClearTypeProcessor();
    controller_ = NULL;
    RunQueuedUIThreadTasks();
  }

 protected:
  void CreateTypeProcessor() {
    // TODO(stanisc): Controller should discover the service via SyncClient.
    type_processor_.reset(
        new syncer_v2::SharedModelTypeProcessor(syncer::DICTIONARY, &service_));
    type_processor_for_ui_ = type_processor_->AsWeakPtrForUI();
  }

  void InitTypeProcessorOnUIThread() {
    CreateTypeProcessor();
    model_thread_runner_ = ui_loop_.task_runner();
    // Don't pass task runner to the controller in this case. It will be making
    // prompt calls instead of posting tasks.
    controller_->Initialize(nullptr, type_processor_for_ui_);
  }

  void InitTypeProcessorOnBackendThread() {
    model_thread_.Start();
    model_thread_runner_ = model_thread_.task_runner();

    // TODO(stanisc): It should be controller's job to
    // create TypeProcessor on the model thread.
    model_thread_runner_->PostTask(
        FROM_HERE,
        base::Bind(&NonBlockingDataTypeControllerTest::CreateTypeProcessor,
                   base::Unretained(this)));
    RunQueuedModelThreadTasks();

    controller_->Initialize(model_thread_runner_, type_processor_for_ui_);
  }

  void ClearTypeProcessor() {
    if (!model_thread_runner_ ||
        model_thread_runner_->BelongsToCurrentThread()) {
      type_processor_.reset();
    } else {
      model_thread_runner_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingDataTypeControllerTest::ClearTypeProcessor,
                     base::Unretained(this)));
      RunQueuedModelThreadTasks();
    }
  }

  void TestTypeProcessor(bool isAllowingChanges, bool isConnected) {
    if (model_thread_runner_->BelongsToCurrentThread()) {
      EXPECT_EQ(isAllowingChanges, type_processor_->IsAllowingChanges());
      EXPECT_EQ(isConnected, type_processor_->IsConnected());
    } else {
      model_thread_runner_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingDataTypeControllerTest::TestTypeProcessor,
                     base::Unretained(this), isAllowingChanges, isConnected));
      RunQueuedModelThreadTasks();
    }
  }

  void OnMetadataLoaded() {
    if (model_thread_runner_->BelongsToCurrentThread()) {
      type_processor_->OnMetadataLoaded(
          make_scoped_ptr(new syncer_v2::MetadataBatch()));
    } else {
      model_thread_runner_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingDataTypeControllerTest::OnMetadataLoaded,
                     base::Unretained(this)));
      RunQueuedModelThreadTasks();
    }
  }

  void LoadModels() {
    OnMetadataLoaded();
    controller_->LoadModels(
        base::Bind(&NonBlockingDataTypeControllerTest::LoadModelsDone,
                   base::Unretained(this)));

    if (auto_run_tasks_) {
      RunAllTasks();
    }
  }

  void StartAssociating() {
    controller_->StartAssociating(
        base::Bind(&NonBlockingDataTypeControllerTest::AssociationDone,
                   base::Unretained(this)));
    // The callback is expected to be promptly called.
    EXPECT_TRUE(association_callback_called_);
  }

  void ActivateDataType() {
    DCHECK(association_callback_called_);
    controller_->ActivateDataType(&configurer_);
    if (auto_run_tasks_) {
      RunAllTasks();
    }
  }

  void DeactivateDataTypeAndStop() {
    controller_->DeactivateDataType(&configurer_);
    controller_->Stop();
    if (auto_run_tasks_) {
      RunAllTasks();
    }
  }

  // These threads can ping-pong for a bit so we run the model thread twice.
  void RunAllTasks() {
    RunQueuedModelThreadTasks();
    RunQueuedUIThreadTasks();
    RunQueuedSyncThreadTasks();
    RunQueuedModelThreadTasks();
  }

  // Runs any tasks posted on UI thread.
  void RunQueuedUIThreadTasks() { ui_loop_.RunUntilIdle(); }

  // Runs any tasks posted on model thread.
  void RunQueuedModelThreadTasks() {
    base::RunLoop run_loop;
    model_thread_runner_->PostTaskAndReply(
        FROM_HERE, base::Bind(&base::DoNothing),
        base::Bind(&base::RunLoop::Quit, base::Unretained(&run_loop)));
    run_loop.Run();
  }

  // Processes any pending connect or disconnect requests and sends
  // responses synchronously.
  void RunQueuedSyncThreadTasks() { sync_thread_runner_->RunUntilIdle(); }

  void SetAutoRunTasks(bool auto_run_tasks) {
    auto_run_tasks_ = auto_run_tasks;
  }

  void LoadModelsDone(syncer::ModelType type, syncer::SyncError error) {
    load_models_callback_called_ = true;
    load_models_error_ = error;
  }

  void AssociationDone(sync_driver::DataTypeController::ConfigureResult result,
                       const syncer::SyncMergeResult& local_merge_result,
                       const syncer::SyncMergeResult& syncer_merge_result) {
    EXPECT_EQ(sync_driver::DataTypeController::OK, result);
    association_callback_called_ = true;
  }

  scoped_ptr<syncer_v2::SharedModelTypeProcessor> type_processor_;
  base::WeakPtr<syncer_v2::SharedModelTypeProcessor> type_processor_for_ui_;
  scoped_refptr<TestController> controller_;

  bool auto_run_tasks_;
  bool load_models_callback_called_;
  syncer::SyncError load_models_error_;
  bool association_callback_called_;
  base::MessageLoopForUI ui_loop_;
  base::Thread model_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> model_thread_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> sync_thread_runner_;
  MockSyncBackend backend_;
  MockBackendDataTypeConfigurer configurer_;
  syncer_v2::FakeModelTypeService service_;
};

TEST_F(NonBlockingDataTypeControllerTest, InitialState) {
  EXPECT_EQ(syncer::DICTIONARY, controller_->type());
  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING, controller_->state());
}

TEST_F(NonBlockingDataTypeControllerTest, LoadModelsOnUIThread) {
  InitTypeProcessorOnUIThread();
  TestTypeProcessor(false, false);  // not enabled, not connected.
  LoadModels();
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            controller_->state());
  EXPECT_TRUE(load_models_callback_called_);
  EXPECT_FALSE(load_models_error_.IsSet());
  TestTypeProcessor(true, false);  // enabled, not connected.
}

TEST_F(NonBlockingDataTypeControllerTest, LoadModelsOnBackendThread) {
  InitTypeProcessorOnBackendThread();
  TestTypeProcessor(false, false);  // not enabled, not connected.
  SetAutoRunTasks(false);
  LoadModels();
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            controller_->state());
  RunAllTasks();
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            controller_->state());
  EXPECT_TRUE(load_models_callback_called_);
  EXPECT_FALSE(load_models_error_.IsSet());
  TestTypeProcessor(true, false);  // enabled, not connected.
}

TEST_F(NonBlockingDataTypeControllerTest, LoadModelsTwice) {
  InitTypeProcessorOnUIThread();
  LoadModels();
  SetAutoRunTasks(false);
  LoadModels();
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            controller_->state());
  // The second LoadModels call should set the error.
  EXPECT_TRUE(load_models_error_.IsSet());
}

TEST_F(NonBlockingDataTypeControllerTest, ActivateDataTypeOnUIThread) {
  InitTypeProcessorOnUIThread();
  LoadModels();
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            controller_->state());

  StartAssociating();
  EXPECT_EQ(sync_driver::DataTypeController::RUNNING, controller_->state());

  ActivateDataType();
  TestTypeProcessor(true, true);  // enabled, connected.
}

TEST_F(NonBlockingDataTypeControllerTest, ActivateDataTypeOnBackendThread) {
  InitTypeProcessorOnBackendThread();
  LoadModels();
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            controller_->state());

  StartAssociating();
  EXPECT_EQ(sync_driver::DataTypeController::RUNNING, controller_->state());

  ActivateDataType();
  TestTypeProcessor(true, true);  // enabled, connected.
}

TEST_F(NonBlockingDataTypeControllerTest, Stop) {
  InitTypeProcessorOnBackendThread();
  LoadModels();
  StartAssociating();
  ActivateDataType();
  TestTypeProcessor(true, true);  // enabled, connected.

  DeactivateDataTypeAndStop();
  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING, controller_->state());
  TestTypeProcessor(true, false);  // enabled, not connected.
}

}  // namespace sync_driver_v2
