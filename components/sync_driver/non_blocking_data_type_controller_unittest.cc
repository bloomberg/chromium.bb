// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "components/sync_driver/non_blocking_data_type_controller.h"
#include "sync/engine/non_blocking_type_processor.h"
#include "sync/engine/non_blocking_type_processor_core_interface.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/sync_core_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class NonBlockingTypeProcessorCore;

namespace {

// A useless instance of NonBlockingTypeProcessorCore.
class NullNonBlockingTypeProcessorCore
    : public NonBlockingTypeProcessorCoreInterface {
 public:
  NullNonBlockingTypeProcessorCore();
  virtual ~NullNonBlockingTypeProcessorCore();

  virtual void RequestCommits(const CommitRequestDataList& list) OVERRIDE;
};

NullNonBlockingTypeProcessorCore::NullNonBlockingTypeProcessorCore() {
}

NullNonBlockingTypeProcessorCore::~NullNonBlockingTypeProcessorCore() {
}

void NullNonBlockingTypeProcessorCore::RequestCommits(
    const CommitRequestDataList& list) {
  NOTREACHED() << "Not implemented.";
}

// A class that pretends to be the sync backend.
class MockSyncCore {
 public:
  void Connect(
      syncer::ModelType type,
      const scoped_refptr<base::SingleThreadTaskRunner>& model_task_runner,
      const base::WeakPtr<syncer::NonBlockingTypeProcessor>& type_processor) {
    enabled_types_.Put(type);
    model_task_runner->PostTask(
        FROM_HERE,
        base::Bind(
            &syncer::NonBlockingTypeProcessor::OnConnect,
            type_processor,
            base::Passed(scoped_ptr<NonBlockingTypeProcessorCoreInterface>(
                             new NullNonBlockingTypeProcessorCore()).Pass())));
  }

  void Disconnect(syncer::ModelType type) {
    DCHECK(enabled_types_.Has(type));
    enabled_types_.Remove(type);
  }

 private:
  std::list<base::Closure> tasks_;
  syncer::ModelTypeSet enabled_types_;
};

// A proxy to the MockSyncCore that implements SyncCoreProxy.
class MockSyncCoreProxy : public syncer::SyncCoreProxy {
 public:
  MockSyncCoreProxy(
      MockSyncCore* sync_core,
      const scoped_refptr<base::TestSimpleTaskRunner>& model_task_runner,
      const scoped_refptr<base::TestSimpleTaskRunner>& sync_task_runner)
      : mock_sync_core_(sync_core),
        model_task_runner_(model_task_runner),
        sync_task_runner_(sync_task_runner) {}
  virtual ~MockSyncCoreProxy() {}

  virtual void ConnectTypeToCore(
      syncer::ModelType type,
      const DataTypeState& data_type_state,
      base::WeakPtr<syncer::NonBlockingTypeProcessor> type_processor) OVERRIDE {
    // Normally we'd use MessageLoopProxy::current() as the TaskRunner argument
    // to Connect().  That won't work here in this test, so we use the
    // model_task_runner_ that was injected for this purpose instead.
    sync_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&MockSyncCore::Connect,
                                           base::Unretained(mock_sync_core_),
                                           type,
                                           model_task_runner_,
                                           type_processor));
  }

  virtual void Disconnect(syncer::ModelType type) OVERRIDE {
    sync_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&MockSyncCore::Disconnect,
                                           base::Unretained(mock_sync_core_),
                                           type));
  }

  virtual scoped_ptr<SyncCoreProxy> Clone() const OVERRIDE {
    return scoped_ptr<SyncCoreProxy>(new MockSyncCoreProxy(
        mock_sync_core_, model_task_runner_, sync_task_runner_));
  }

 private:
  MockSyncCore* mock_sync_core_;
  scoped_refptr<base::TestSimpleTaskRunner> model_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> sync_task_runner_;
};

}  // namespace

class NonBlockingDataTypeControllerTest : public testing::Test {
 public:
  NonBlockingDataTypeControllerTest()
      : processor_(syncer::DICTIONARY),
        model_thread_(new base::TestSimpleTaskRunner()),
        sync_thread_(new base::TestSimpleTaskRunner()),
        controller_(syncer::DICTIONARY, true),
        mock_core_proxy_(&mock_sync_core_, model_thread_, sync_thread_),
        auto_run_tasks_(true) {}

  virtual ~NonBlockingDataTypeControllerTest() {}

  // Connects the processor to the NonBlockingDataTypeController.
  void InitProcessor() {
    controller_.InitializeProcessor(model_thread_, processor_.AsWeakPtrForUI());
    if (auto_run_tasks_) {
      RunAllTasks();
    }
  }

  // Connects the sync backend to the NonBlockingDataTypeController.
  void InitSync() {
    controller_.InitializeSyncCoreProxy(mock_core_proxy_.Clone());
    if (auto_run_tasks_) {
      RunAllTasks();
    }
  }

  // Disconnects the sync backend from the NonBlockingDataTypeController.
  void UninitializeSync() {
    controller_.ClearSyncCoreProxy();
    if (auto_run_tasks_) {
      RunAllTasks();
    }
  }

  // Toggles the user's preference for syncing this type.
  void SetIsPreferred(bool preferred) {
    controller_.SetIsPreferred(preferred);
    if (auto_run_tasks_) {
      RunAllTasks();
    }
  }

  // These threads can ping-pong for a bit so we run the model thread twice.
  void RunAllTasks() {
    RunQueuedModelThreadTasks();
    RunQueuedSyncThreadTasks();
    RunQueuedModelThreadTasks();
  }

  // The processor pretends to run tasks on a different thread.
  // This function runs any posted tasks.
  void RunQueuedModelThreadTasks() { model_thread_->RunUntilIdle(); }

  // Processes any pending connect or disconnect requests and sends
  // responses synchronously.
  void RunQueuedSyncThreadTasks() { sync_thread_->RunUntilIdle(); }

  void SetAutoRunTasks(bool auto_run_tasks) {
    auto_run_tasks_ = auto_run_tasks;
  }

 protected:
  syncer::NonBlockingTypeProcessor processor_;
  scoped_refptr<base::TestSimpleTaskRunner> model_thread_;
  scoped_refptr<base::TestSimpleTaskRunner> sync_thread_;

  browser_sync::NonBlockingDataTypeController controller_;

  MockSyncCore mock_sync_core_;
  MockSyncCoreProxy mock_core_proxy_;

  bool auto_run_tasks_;
};

// Initialization when the user has disabled syncing for this type.
TEST_F(NonBlockingDataTypeControllerTest, UserDisabled) {
  SetIsPreferred(false);
  InitProcessor();
  InitSync();

  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());

  UninitializeSync();

  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());
}

// Init the sync backend then the type processor.
TEST_F(NonBlockingDataTypeControllerTest, Enabled_SyncFirst) {
  SetIsPreferred(true);
  InitSync();
  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());

  InitProcessor();
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_TRUE(processor_.IsConnected());

  UninitializeSync();
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());
}

// Init the type processor then the sync backend.
TEST_F(NonBlockingDataTypeControllerTest, Enabled_ProcessorFirst) {
  SetIsPreferred(true);
  InitProcessor();
  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());

  InitSync();
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_TRUE(processor_.IsConnected());

  UninitializeSync();
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());
}

// Initialize sync then disable it with a pref change.
TEST_F(NonBlockingDataTypeControllerTest, PreferThenNot) {
  SetIsPreferred(true);
  InitProcessor();
  InitSync();

  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_TRUE(processor_.IsConnected());

  SetIsPreferred(false);
  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());
}

// Connect type processor and sync backend, then toggle prefs repeatedly.
TEST_F(NonBlockingDataTypeControllerTest, RepeatedTogglePreference) {
  SetIsPreferred(false);
  InitProcessor();
  InitSync();
  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());

  SetIsPreferred(true);
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_TRUE(processor_.IsConnected());

  SetIsPreferred(false);
  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());

  SetIsPreferred(true);
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_TRUE(processor_.IsConnected());

  SetIsPreferred(false);
  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());
}

// Test sync backend getting restarted while processor is connected.
TEST_F(NonBlockingDataTypeControllerTest, RestartSyncBackend) {
  SetIsPreferred(true);
  InitProcessor();
  InitSync();
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_TRUE(processor_.IsConnected());

  // Shutting down sync backend should disconnect but not disable the type.
  UninitializeSync();
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());

  // Brining the backend back should reconnect the type.
  InitSync();
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_TRUE(processor_.IsConnected());
}

// Test sync backend being restarted before processor connects.
TEST_F(NonBlockingDataTypeControllerTest, RestartSyncBackendEarly) {
  SetIsPreferred(true);

  // Toggle sync off and on before the type processor is available.
  InitSync();
  EXPECT_FALSE(processor_.IsConnected());
  UninitializeSync();
  EXPECT_FALSE(processor_.IsConnected());
  InitSync();
  EXPECT_FALSE(processor_.IsConnected());

  // Introduce the processor.
  InitProcessor();
  EXPECT_TRUE(processor_.IsConnected());
}

// Test pref toggling before the sync backend has connected.
TEST_F(NonBlockingDataTypeControllerTest, TogglePreferenceWithoutBackend) {
  SetIsPreferred(true);
  InitProcessor();

  // This should emit a disable signal.
  SetIsPreferred(false);
  EXPECT_FALSE(processor_.IsConnected());
  EXPECT_FALSE(processor_.IsPreferred());

  // This won't enable us, since we don't have a sync backend.
  SetIsPreferred(true);
  EXPECT_FALSE(processor_.IsConnected());
  EXPECT_FALSE(processor_.IsPreferred());

  // Only now do we start sending enable signals.
  InitSync();
  EXPECT_TRUE(processor_.IsConnected());
  EXPECT_TRUE(processor_.IsPreferred());
}

// Turns off auto-task-running to test the effects of delaying a connection
// response.
//
// This is mostly a test of the test framework.  It's not very interesting on
// its own, but it provides a useful "control" against some of the more
// complicated race tests below.
TEST_F(NonBlockingDataTypeControllerTest, DelayedConnect) {
  SetAutoRunTasks(false);

  SetIsPreferred(true);
  InitProcessor();
  InitSync();

  // Allow the model to emit the request.
  RunQueuedModelThreadTasks();

  // That should result in a request to connect, but it won't be
  // executed right away.
  EXPECT_FALSE(processor_.IsConnected());
  EXPECT_TRUE(processor_.IsPreferred());

  // Let the sync thread process the request and the model thread handle its
  // response.
  RunQueuedSyncThreadTasks();
  RunQueuedModelThreadTasks();

  EXPECT_TRUE(processor_.IsConnected());
  EXPECT_TRUE(processor_.IsPreferred());
}

// Send Disable signal while a connection request is in progress.
TEST_F(NonBlockingDataTypeControllerTest, DisableRacesWithOnConnect) {
  SetAutoRunTasks(false);

  SetIsPreferred(true);
  InitProcessor();
  InitSync();

  // Allow the model to emit the request.
  RunQueuedModelThreadTasks();

  // That should result in a request to connect, but it won't be
  // executed right away.
  EXPECT_FALSE(processor_.IsConnected());
  EXPECT_TRUE(processor_.IsPreferred());

  // Send and execute a disable signal before the OnConnect callback returns.
  SetIsPreferred(false);

  // Now we let sync process the initial request and the disable request,
  // both of which should be sitting in its queue.
  RunQueuedSyncThreadTasks();

  // Let the model thread process any responses received from the sync thread.
  // A plausible error would be that the sync thread returns a "connection OK"
  // message, and this message overrides the request to disable that arrived
  // from the UI thread earlier.  We need to make sure that doesn't happen.
  RunQueuedModelThreadTasks();

  EXPECT_FALSE(processor_.IsPreferred());
  EXPECT_FALSE(processor_.IsConnected());
}

// Send a request to enable, then disable, then re-enable the data type.
//
// To make it more interesting, we stall the sync thread until all three
// requests have been passed to the model thread.
TEST_F(NonBlockingDataTypeControllerTest, EnableDisableEnableRace) {
  SetAutoRunTasks(false);

  SetIsPreferred(true);
  InitProcessor();
  InitSync();
  RunQueuedModelThreadTasks();

  // That was the first enable.
  EXPECT_FALSE(processor_.IsConnected());
  EXPECT_TRUE(processor_.IsPreferred());

  // Now disable.
  SetIsPreferred(false);
  RunQueuedModelThreadTasks();
  EXPECT_FALSE(processor_.IsPreferred());

  // And re-enable.
  SetIsPreferred(true);
  RunQueuedModelThreadTasks();
  EXPECT_TRUE(processor_.IsPreferred());

  // The sync thread has three messages related to those enables and
  // disables sittin in its queue.  Let's allow it to process them.
  RunQueuedSyncThreadTasks();

  // Let the model thread process any messages from the sync thread.
  RunQueuedModelThreadTasks();
  EXPECT_TRUE(processor_.IsPreferred());
  EXPECT_TRUE(processor_.IsConnected());
}

}  // namespace syncer
