// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/non_ui_model_type_controller.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "components/sync/api/stub_model_type_service.h"
#include "components/sync/core/activation_context.h"
#include "components/sync/core/shared_model_type_processor.h"
#include "components/sync/driver/backend_data_type_configurer.h"
#include "components/sync/driver/fake_sync_client.h"
#include "components/sync/engine/commit_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

// Test controller derived from NonUIModelTypeController.
class TestNonUIModelTypeController : public NonUIModelTypeController {
 public:
  TestNonUIModelTypeController(
      ModelType model_type,
      const scoped_refptr<base::TaskRunner>& model_task_runner,
      const base::Closure& dump_stack,
      SyncClient* sync_client)
      : NonUIModelTypeController(model_type, dump_stack, sync_client),
        model_task_runner_(model_task_runner) {}
  ~TestNonUIModelTypeController() override {}

  bool RunOnModelThread(const tracked_objects::Location& from_here,
                        const base::Closure& task) override {
    DCHECK(model_task_runner_);
    return model_task_runner_->PostTask(from_here, task);
  }

 private:
  scoped_refptr<base::TaskRunner> model_task_runner_;
};

// A no-op instance of CommitQueue.
class NullCommitQueue : public CommitQueue {
 public:
  NullCommitQueue() {}
  ~NullCommitQueue() override {}

  void EnqueueForCommit(const CommitRequestDataList& list) override {
    NOTREACHED() << "Not implemented.";
  }
};

// A class that pretends to be the sync backend.
class MockSyncBackend {
 public:
  void Connect(ModelType type,
               std::unique_ptr<ActivationContext> activation_context) {
    enabled_types_.Put(type);
    activation_context->type_processor->ConnectSync(
        base::MakeUnique<NullCommitQueue>());
  }

  void Disconnect(ModelType type) {
    DCHECK(enabled_types_.Has(type));
    enabled_types_.Remove(type);
  }

 private:
  ModelTypeSet enabled_types_;
};

// Fake implementation of BackendDataTypeConfigurer that pretends to be Sync
// backend.
class MockBackendDataTypeConfigurer : public BackendDataTypeConfigurer {
 public:
  MockBackendDataTypeConfigurer(
      MockSyncBackend* backend,
      const scoped_refptr<base::TaskRunner>& sync_task_runner)
      : backend_(backend), sync_task_runner_(sync_task_runner) {}
  ~MockBackendDataTypeConfigurer() override {}

  ModelTypeSet ConfigureDataTypes(
      ConfigureReason reason,
      const DataTypeConfigStateMap& config_state_map,
      const base::Callback<void(ModelTypeSet, ModelTypeSet)>& ready_task,
      const base::Callback<void()>& retry_callback) override {
    NOTREACHED() << "Not implemented.";
    return ModelTypeSet();
  }

  void ActivateDirectoryDataType(ModelType type,
                                 ModelSafeGroup group,
                                 ChangeProcessor* change_processor) override {
    NOTREACHED() << "Not implemented.";
  }

  void DeactivateDirectoryDataType(ModelType type) override {
    NOTREACHED() << "Not implemented.";
  }

  void ActivateNonBlockingDataType(
      ModelType type,
      std::unique_ptr<ActivationContext> activation_context) override {
    // Post on Sync thread just like the real implementation does.
    sync_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MockSyncBackend::Connect, base::Unretained(backend_), type,
                   base::Passed(std::move(activation_context))));
  }

  void DeactivateNonBlockingDataType(ModelType type) override {
    sync_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&MockSyncBackend::Disconnect,
                                           base::Unretained(backend_), type));
  }

 private:
  MockSyncBackend* backend_;
  scoped_refptr<base::TaskRunner> sync_task_runner_;
};

}  // namespace

class NonUIModelTypeControllerTest : public testing::Test,
                                     public FakeSyncClient {
 public:
  NonUIModelTypeControllerTest()
      : auto_run_tasks_(true),
        load_models_callback_called_(false),
        association_callback_called_(false),
        model_thread_("modelthread"),
        configurer_(&backend_, ui_loop_.task_runner()) {}

  ~NonUIModelTypeControllerTest() override {}

  void SetUp() override {
    model_thread_.Start();
    model_thread_runner_ = model_thread_.task_runner();
    InitializeModelTypeService();
    controller_.reset(new TestNonUIModelTypeController(
        DICTIONARY, model_thread_runner_, base::Closure(), this));
  }

  void TearDown() override {
    ClearModelTypeService();
    RunQueuedUIThreadTasks();
  }

  base::WeakPtr<ModelTypeService> GetModelTypeServiceForType(
      ModelType type) override {
    return service_->AsWeakPtr();
  }

 protected:
  std::unique_ptr<ModelTypeChangeProcessor> CreateProcessor(
      ModelType type,
      ModelTypeService* service) {
    std::unique_ptr<SharedModelTypeProcessor> processor =
        base::MakeUnique<SharedModelTypeProcessor>(type, service);
    type_processor_ = processor.get();
    return std::move(processor);
  }

  void InitializeModelTypeService() {
    if (!model_thread_runner_ ||
        model_thread_runner_->BelongsToCurrentThread()) {
      service_.reset(new StubModelTypeService(
          base::Bind(&NonUIModelTypeControllerTest::CreateProcessor,
                     base::Unretained(this))));
    } else {
      model_thread_runner_->PostTask(
          FROM_HERE,
          base::Bind(&NonUIModelTypeControllerTest::InitializeModelTypeService,
                     base::Unretained(this)));
      RunQueuedModelThreadTasks();
    }
  }

  void ClearModelTypeService() {
    if (!model_thread_runner_ ||
        model_thread_runner_->BelongsToCurrentThread()) {
      service_.reset();
    } else {
      model_thread_runner_->PostTask(
          FROM_HERE,
          base::Bind(&NonUIModelTypeControllerTest::ClearModelTypeService,
                     base::Unretained(this)));
      RunQueuedModelThreadTasks();
    }
  }

  void ExpectProcessorConnected(bool isConnected) {
    if (model_thread_runner_->BelongsToCurrentThread()) {
      DCHECK(type_processor_);
      EXPECT_EQ(isConnected, type_processor_->IsConnected());
    } else {
      model_thread_runner_->PostTask(
          FROM_HERE,
          base::Bind(&NonUIModelTypeControllerTest::ExpectProcessorConnected,
                     base::Unretained(this), isConnected));
      RunQueuedModelThreadTasks();
    }
  }

  void OnMetadataLoaded() {
    if (model_thread_runner_->BelongsToCurrentThread()) {
      if (!type_processor_->IsAllowingChanges()) {
        type_processor_->OnMetadataLoaded(SyncError(),
                                          base::MakeUnique<MetadataBatch>());
      }
    } else {
      model_thread_runner_->PostTask(
          FROM_HERE, base::Bind(&NonUIModelTypeControllerTest::OnMetadataLoaded,
                                base::Unretained(this)));
    }
  }

  void LoadModels() {
    controller_->LoadModels(base::Bind(
        &NonUIModelTypeControllerTest::LoadModelsDone, base::Unretained(this)));
    OnMetadataLoaded();

    if (auto_run_tasks_) {
      RunAllTasks();
    }
  }

  void RegisterWithBackend() {
    controller_->RegisterWithBackend(&configurer_);
    if (auto_run_tasks_) {
      RunAllTasks();
    }
  }

  void StartAssociating() {
    controller_->StartAssociating(
        base::Bind(&NonUIModelTypeControllerTest::AssociationDone,
                   base::Unretained(this)));
    // The callback is expected to be promptly called.
    EXPECT_TRUE(association_callback_called_);
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
    RunQueuedModelThreadTasks();
  }

  // Runs any tasks posted on UI thread.
  void RunQueuedUIThreadTasks() { base::RunLoop().RunUntilIdle(); }

  // Runs any tasks posted on model thread.
  void RunQueuedModelThreadTasks() {
    base::RunLoop run_loop;
    model_thread_runner_->PostTaskAndReply(
        FROM_HERE, base::Bind(&base::DoNothing),
        base::Bind(&base::RunLoop::Quit, base::Unretained(&run_loop)));
    run_loop.Run();
  }

  void SetAutoRunTasks(bool auto_run_tasks) {
    auto_run_tasks_ = auto_run_tasks;
  }

  void LoadModelsDone(ModelType type, const SyncError& error) {
    load_models_callback_called_ = true;
    load_models_error_ = error;
  }

  void AssociationDone(DataTypeController::ConfigureResult result,
                       const SyncMergeResult& local_merge_result,
                       const SyncMergeResult& syncer_merge_result) {
    EXPECT_EQ(DataTypeController::OK, result);
    association_callback_called_ = true;
  }

  SharedModelTypeProcessor* type_processor_;
  std::unique_ptr<TestNonUIModelTypeController> controller_;

  bool auto_run_tasks_;
  bool load_models_callback_called_;
  SyncError load_models_error_;
  bool association_callback_called_;
  base::MessageLoopForUI ui_loop_;
  base::Thread model_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> model_thread_runner_;
  MockSyncBackend backend_;
  MockBackendDataTypeConfigurer configurer_;
  std::unique_ptr<StubModelTypeService> service_;
};

TEST_F(NonUIModelTypeControllerTest, InitialState) {
  EXPECT_EQ(DICTIONARY, controller_->type());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller_->state());
}

TEST_F(NonUIModelTypeControllerTest, LoadModelsOnBackendThread) {
  SetAutoRunTasks(false);
  LoadModels();
  EXPECT_EQ(DataTypeController::MODEL_STARTING, controller_->state());
  RunAllTasks();
  EXPECT_EQ(DataTypeController::MODEL_LOADED, controller_->state());
  EXPECT_TRUE(load_models_callback_called_);
  EXPECT_FALSE(load_models_error_.IsSet());
  ExpectProcessorConnected(false);
}

TEST_F(NonUIModelTypeControllerTest, LoadModelsTwice) {
  LoadModels();
  SetAutoRunTasks(false);
  LoadModels();
  EXPECT_EQ(DataTypeController::MODEL_LOADED, controller_->state());
  // The second LoadModels call should set the error.
  EXPECT_TRUE(load_models_error_.IsSet());
}

TEST_F(NonUIModelTypeControllerTest, ActivateDataTypeOnBackendThread) {
  LoadModels();
  EXPECT_EQ(DataTypeController::MODEL_LOADED, controller_->state());
  RegisterWithBackend();
  ExpectProcessorConnected(true);

  StartAssociating();
  EXPECT_EQ(DataTypeController::RUNNING, controller_->state());
}

TEST_F(NonUIModelTypeControllerTest, Stop) {
  LoadModels();
  RegisterWithBackend();
  ExpectProcessorConnected(true);

  StartAssociating();

  DeactivateDataTypeAndStop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller_->state());
}

}  // namespace syncer
