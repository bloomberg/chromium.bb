// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/core_impl/model_type_connector_proxy.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/api/data_type_error_handler_mock.h"
#include "components/sync/api/stub_model_type_service.h"
#include "components/sync/core/activation_context.h"
#include "components/sync/core/shared_model_type_processor.h"
#include "components/sync/engine_impl/model_type_registry.h"
#include "components/sync/test/engine/mock_nudge_handler.h"
#include "components/sync/test/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class ModelTypeConnectorProxyTest : public ::testing::Test,
                                    StubModelTypeService {
 public:
  ModelTypeConnectorProxyTest()
      : sync_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        type_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

  void SetUp() override {
    dir_maker_.SetUp();
    registry_.reset(new ModelTypeRegistry(workers_, dir_maker_.directory(),
                                          &nudge_handler_));
    connector_proxy_.reset(
        new ModelTypeConnectorProxy(sync_task_runner_, registry_->AsWeakPtr()));
  }

  void TearDown() override {
    connector_proxy_.reset();
    registry_.reset();
    dir_maker_.TearDown();
  }

  // The sync thread could be shut down at any time without warning.  This
  // function simulates such an event.
  void DisableSync() { registry_.reset(); }

  void OnSyncStarting(SharedModelTypeProcessor* processor) {
    processor->OnSyncStarting(
        base::MakeUnique<DataTypeErrorHandlerMock>(),
        base::Bind(&ModelTypeConnectorProxyTest::OnReadyToConnect,
                   base::Unretained(this)));
  }

  void OnReadyToConnect(SyncError error,
                        std::unique_ptr<ActivationContext> context) {
    connector_proxy_->ConnectType(THEMES, std::move(context));
  }

  std::unique_ptr<SharedModelTypeProcessor> CreateModelTypeProcessor() {
    std::unique_ptr<SharedModelTypeProcessor> processor =
        base::WrapUnique(new SharedModelTypeProcessor(THEMES, this));
    processor->OnMetadataLoaded(SyncError(), base::MakeUnique<MetadataBatch>());
    return processor;
  }

 private:
  base::MessageLoop loop_;
  scoped_refptr<base::SingleThreadTaskRunner> sync_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> type_task_runner_;

  std::vector<scoped_refptr<ModelSafeWorker>> workers_;
  TestDirectorySetterUpper dir_maker_;
  MockNudgeHandler nudge_handler_;
  std::unique_ptr<ModelTypeRegistry> registry_;

  std::unique_ptr<ModelTypeConnectorProxy> connector_proxy_;
};

// Try to connect a type to a ModelTypeConnector that has already shut down.
TEST_F(ModelTypeConnectorProxyTest, FailToConnect1) {
  std::unique_ptr<SharedModelTypeProcessor> processor =
      CreateModelTypeProcessor();
  DisableSync();
  OnSyncStarting(processor.get());

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();
  EXPECT_FALSE(processor->IsConnected());
}

// Try to connect a type to a ModelTypeConnector as it shuts down.
TEST_F(ModelTypeConnectorProxyTest, FailToConnect2) {
  std::unique_ptr<SharedModelTypeProcessor> processor =
      CreateModelTypeProcessor();
  OnSyncStarting(processor.get());
  DisableSync();

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();
  EXPECT_FALSE(processor->IsConnected());
}

// Tests the case where the type's sync proxy shuts down first.
TEST_F(ModelTypeConnectorProxyTest, TypeDisconnectsFirst) {
  std::unique_ptr<SharedModelTypeProcessor> processor =
      CreateModelTypeProcessor();
  OnSyncStarting(processor.get());

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();

  EXPECT_TRUE(processor->IsConnected());
  processor.reset();
}

// Tests the case where the sync thread shuts down first.
TEST_F(ModelTypeConnectorProxyTest, SyncDisconnectsFirst) {
  std::unique_ptr<SharedModelTypeProcessor> processor =
      CreateModelTypeProcessor();
  OnSyncStarting(processor.get());

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();

  EXPECT_TRUE(processor->IsConnected());
  DisableSync();
}

}  // namespace syncer
