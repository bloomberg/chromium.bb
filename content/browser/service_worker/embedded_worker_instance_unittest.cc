// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include <stdint.h>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/public/common/child_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

void SaveStatusAndCall(ServiceWorkerStatusCode* out,
                       const base::Closure& callback,
                       ServiceWorkerStatusCode status) {
  *out = status;
  callback.Run();
}

}  // namespace

class EmbeddedWorkerInstanceTest : public testing::Test,
                                   public EmbeddedWorkerInstance::Listener {
 protected:
  EmbeddedWorkerInstanceTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  enum EventType {
    PROCESS_ALLOCATED,
    START_WORKER_MESSAGE_SENT,
    STARTED,
    STOPPED,
    DETACHED,
  };

  struct EventLog {
    EventType type;
    EmbeddedWorkerInstance::Status status;
  };

  void RecordEvent(
      EventType type,
      EmbeddedWorkerInstance::Status status = EmbeddedWorkerInstance::STOPPED) {
    EventLog log = {type, status};
    events_.push_back(log);
  }

  void OnProcessAllocated() override { RecordEvent(PROCESS_ALLOCATED); }
  void OnStartWorkerMessageSent() override {
    RecordEvent(START_WORKER_MESSAGE_SENT);
  }
  void OnStarted() override { RecordEvent(STARTED); }
  void OnStopped(EmbeddedWorkerInstance::Status old_status) override {
    RecordEvent(STOPPED, old_status);
  }
  void OnDetached(EmbeddedWorkerInstance::Status old_status) override {
    RecordEvent(DETACHED, old_status);
  }

  bool OnMessageReceived(const IPC::Message& message) override { return false; }

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  void TearDown() override { helper_.reset(); }

  ServiceWorkerStatusCode StartWorker(EmbeddedWorkerInstance* worker,
                                      int id, const GURL& pattern,
                                      const GURL& url) {
    ServiceWorkerStatusCode status;
    base::RunLoop run_loop;
    worker->Start(id, pattern, url, base::Bind(&SaveStatusAndCall, &status,
                                               run_loop.QuitClosure()));
    run_loop.Run();
    return status;
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }

  EmbeddedWorkerRegistry* embedded_worker_registry() {
    DCHECK(context());
    return context()->embedded_worker_registry();
  }

  IPC::TestSink* ipc_sink() { return helper_->ipc_sink(); }

  TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  std::vector<EventLog> events_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstanceTest);
};

// A helper to simulate the start worker sequence is stalled in a worker
// process.
class StalledInStartWorkerHelper : public EmbeddedWorkerTestHelper {
 public:
  StalledInStartWorkerHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~StalledInStartWorkerHelper() override{};

  void OnStartWorker(int embedded_worker_id,
                     int64_t service_worker_version_id,
                     const GURL& scope,
                     const GURL& script_url) override {
    if (force_stall_in_start_) {
      // Do nothing to simulate a stall in the worker process.
      return;
    }
    EmbeddedWorkerTestHelper::OnStartWorker(
        embedded_worker_id, service_worker_version_id, scope, script_url);
  }

  void set_force_stall_in_start(bool force_stall_in_start) {
    force_stall_in_start_ = force_stall_in_start;
  }

 private:
  bool force_stall_in_start_ = true;
};

class FailToSendIPCHelper : public EmbeddedWorkerTestHelper {
 public:
  FailToSendIPCHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~FailToSendIPCHelper() override {}

  bool Send(IPC::Message* message) override {
    delete message;
    return false;
  }
};

TEST_F(EmbeddedWorkerInstanceTest, StartAndStop) {
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  const int64_t service_worker_version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");

  // Simulate adding one process to the pattern.
  helper_->SimulateAddProcessToPattern(pattern,
                                       helper_->mock_render_process_id());

  // Start should succeed.
  ServiceWorkerStatusCode status;
  base::RunLoop run_loop;
  worker->Start(
      service_worker_version_id,
      pattern,
      url,
      base::Bind(&SaveStatusAndCall, &status, run_loop.QuitClosure()));
  EXPECT_EQ(EmbeddedWorkerInstance::STARTING, worker->status());
  run_loop.Run();
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  // The 'WorkerStarted' message should have been sent by
  // EmbeddedWorkerTestHelper.
  EXPECT_EQ(EmbeddedWorkerInstance::RUNNING, worker->status());
  EXPECT_EQ(helper_->mock_render_process_id(), worker->process_id());

  // Stop the worker.
  EXPECT_EQ(SERVICE_WORKER_OK, worker->Stop());
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPING, worker->status());
  base::RunLoop().RunUntilIdle();

  // The 'WorkerStopped' message should have been sent by
  // EmbeddedWorkerTestHelper.
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  // Verify that we've sent two messages to start and terminate the worker.
  ASSERT_TRUE(
      ipc_sink()->GetUniqueMessageMatching(EmbeddedWorkerMsg_StartWorker::ID));
  ASSERT_TRUE(ipc_sink()->GetUniqueMessageMatching(
      EmbeddedWorkerMsg_StopWorker::ID));
}

TEST_F(EmbeddedWorkerInstanceTest, StopWhenDevToolsAttached) {
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  const int64_t service_worker_version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");

  // Simulate adding one process to the pattern.
  helper_->SimulateAddProcessToPattern(pattern,
                                       helper_->mock_render_process_id());

  // Start the worker and then call StopIfIdle().
  EXPECT_EQ(SERVICE_WORKER_OK,
            StartWorker(worker.get(), service_worker_version_id, pattern, url));
  EXPECT_EQ(EmbeddedWorkerInstance::RUNNING, worker->status());
  EXPECT_EQ(helper_->mock_render_process_id(), worker->process_id());
  worker->StopIfIdle();
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPING, worker->status());
  base::RunLoop().RunUntilIdle();

  // The worker must be stopped now.
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  // Set devtools_attached to true, and do the same.
  worker->set_devtools_attached(true);

  EXPECT_EQ(SERVICE_WORKER_OK,
            StartWorker(worker.get(), service_worker_version_id, pattern, url));
  EXPECT_EQ(EmbeddedWorkerInstance::RUNNING, worker->status());
  EXPECT_EQ(helper_->mock_render_process_id(), worker->process_id());
  worker->StopIfIdle();
  base::RunLoop().RunUntilIdle();

  // The worker must not be stopped this time.
  EXPECT_EQ(EmbeddedWorkerInstance::RUNNING, worker->status());

  // Calling Stop() actually stops the worker regardless of whether devtools
  // is attached or not.
  EXPECT_EQ(SERVICE_WORKER_OK, worker->Stop());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());
}

// Test that the removal of a worker from the registry doesn't remove
// other workers in the same process.
TEST_F(EmbeddedWorkerInstanceTest, RemoveWorkerInSharedProcess) {
  scoped_ptr<EmbeddedWorkerInstance> worker1 =
      embedded_worker_registry()->CreateWorker();
  scoped_ptr<EmbeddedWorkerInstance> worker2 =
      embedded_worker_registry()->CreateWorker();

  const int64_t version_id1 = 55L;
  const int64_t version_id2 = 56L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");
  int process_id = helper_->mock_render_process_id();

  helper_->SimulateAddProcessToPattern(pattern, process_id);
  {
    // Start worker1.
    ServiceWorkerStatusCode status;
    base::RunLoop run_loop;
    worker1->Start(
        version_id1, pattern, url,
        base::Bind(&SaveStatusAndCall, &status, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(SERVICE_WORKER_OK, status);
  }

  {
    // Start worker2.
    ServiceWorkerStatusCode status;
    base::RunLoop run_loop;
    worker2->Start(
        version_id2, pattern, url,
        base::Bind(&SaveStatusAndCall, &status, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(SERVICE_WORKER_OK, status);
  }

  // The two workers share the same process.
  EXPECT_EQ(worker1->process_id(), worker2->process_id());

  // Destroy worker1. It removes itself from the registry.
  int worker1_id = worker1->embedded_worker_id();
  worker1->Stop();
  worker1.reset();

  // Only worker1 should be removed from the registry's process_map.
  EmbeddedWorkerRegistry* registry =
      helper_->context()->embedded_worker_registry();
  EXPECT_EQ(0UL, registry->worker_process_map_[process_id].count(worker1_id));
  EXPECT_EQ(1UL, registry->worker_process_map_[process_id].count(
                     worker2->embedded_worker_id()));

  worker2->Stop();
}

TEST_F(EmbeddedWorkerInstanceTest, DetachDuringProcessAllocation) {
  const int64_t version_id = 55L;
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  worker->AddListener(this);

  // Run the start worker sequence and detach during process allocation.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  worker->Start(
      version_id, scope, url,
      base::Bind(&SaveStatusAndCall, &status, base::Bind(&base::DoNothing)));
  worker->Detach();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, worker->process_id());

  // The start callback should not be aborted by detach (see a comment on the
  // dtor of EmbeddedWorkerInstance::StartTask).
  EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, status);

  // "PROCESS_ALLOCATED" event should not be recorded.
  ASSERT_EQ(1u, events_.size());
  EXPECT_EQ(DETACHED, events_[0].type);
  EXPECT_EQ(EmbeddedWorkerInstance::STARTING, events_[0].status);
}

TEST_F(EmbeddedWorkerInstanceTest, DetachAfterSendingStartWorkerMessage) {
  const int64_t version_id = 55L;
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  helper_.reset(new StalledInStartWorkerHelper());
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  worker->AddListener(this);

  // Run the start worker sequence until a start worker message is sent.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  worker->Start(
      version_id, scope, url,
      base::Bind(&SaveStatusAndCall, &status, base::Bind(base::DoNothing)));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  events_.clear();

  worker->Detach();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, worker->process_id());

  // The start callback should not be aborted by detach (see a comment on the
  // dtor of EmbeddedWorkerInstance::StartTask).
  EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, status);

  // "STARTED" event should not be recorded.
  ASSERT_EQ(1u, events_.size());
  EXPECT_EQ(DETACHED, events_[0].type);
  EXPECT_EQ(EmbeddedWorkerInstance::STARTING, events_[0].status);
}

TEST_F(EmbeddedWorkerInstanceTest, StopDuringProcessAllocation) {
  const int64_t version_id = 55L;
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  worker->AddListener(this);

  // Stop the start worker sequence before a process is allocated.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  worker->Start(
      version_id, scope, url,
      base::Bind(&SaveStatusAndCall, &status, base::Bind(base::DoNothing)));
  worker->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, worker->process_id());

  // The start callback should not be aborted by stop (see a comment on the dtor
  // of EmbeddedWorkerInstance::StartTask).
  EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, status);

  // "PROCESS_ALLOCATED" event should not be recorded.
  ASSERT_EQ(1u, events_.size());
  EXPECT_EQ(DETACHED, events_[0].type);
  EXPECT_EQ(EmbeddedWorkerInstance::STARTING, events_[0].status);
  events_.clear();

  // Restart the worker.
  status = SERVICE_WORKER_ERROR_MAX_VALUE;
  scoped_ptr<base::RunLoop> run_loop(new base::RunLoop);
  worker->Start(version_id, scope, url, base::Bind(&SaveStatusAndCall, &status,
                                                   run_loop->QuitClosure()));
  run_loop->Run();

  EXPECT_EQ(SERVICE_WORKER_OK, status);
  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(STARTED, events_[2].type);

  // Tear down the worker.
  worker->Stop();
}

TEST_F(EmbeddedWorkerInstanceTest, StopAfterSendingStartWorkerMessage) {
  const int64_t version_id = 55L;
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  helper_.reset(new StalledInStartWorkerHelper);
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  worker->AddListener(this);

  // Run the start worker sequence until a start worker message is sent.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  worker->Start(
      version_id, scope, url,
      base::Bind(&SaveStatusAndCall, &status, base::Bind(base::DoNothing)));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  events_.clear();

  worker->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, worker->process_id());

  // The start callback should not be aborted by stop (see a comment on the dtor
  // of EmbeddedWorkerInstance::StartTask).
  EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, status);

  // "STARTED" event should not be recorded.
  ASSERT_EQ(1u, events_.size());
  EXPECT_EQ(STOPPED, events_[0].type);
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPING, events_[0].status);
  events_.clear();

  // Restart the worker.
  static_cast<StalledInStartWorkerHelper*>(helper_.get())
      ->set_force_stall_in_start(false);
  status = SERVICE_WORKER_ERROR_MAX_VALUE;
  scoped_ptr<base::RunLoop> run_loop(new base::RunLoop);
  worker->Start(version_id, scope, url, base::Bind(&SaveStatusAndCall, &status,
                                                   run_loop->QuitClosure()));
  run_loop->Run();

  // The worker should be started.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(STARTED, events_[2].type);

  // Tear down the worker.
  worker->Stop();
}

TEST_F(EmbeddedWorkerInstanceTest, Detach) {
  const int64_t version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  helper_->SimulateAddProcessToPattern(pattern,
                                       helper_->mock_render_process_id());
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  worker->AddListener(this);

  // Start the worker.
  base::RunLoop run_loop;
  worker->Start(
      version_id, pattern, url,
      base::Bind(&SaveStatusAndCall, &status, run_loop.QuitClosure()));
  run_loop.Run();

  // Detach.
  int process_id = worker->process_id();
  worker->Detach();
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  // Send the registry a message from the detached worker. Nothing should
  // happen.
  embedded_worker_registry()->OnWorkerStarted(process_id,
                                              worker->embedded_worker_id());
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());
}

// Test for when sending the start IPC failed.
TEST_F(EmbeddedWorkerInstanceTest, FailToSendStartIPC) {
  helper_.reset(new FailToSendIPCHelper());

  const int64_t version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");

  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  helper_->SimulateAddProcessToPattern(pattern,
                                       helper_->mock_render_process_id());
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  worker->AddListener(this);

  // Attempt to start the worker.
  base::RunLoop run_loop;
  worker->Start(
      version_id, pattern, url,
      base::Bind(&SaveStatusAndCall, &status, run_loop.QuitClosure()));
  run_loop.Run();

  // The callback should have run, and we should have got an OnStopped message.
  EXPECT_EQ(SERVICE_WORKER_ERROR_IPC_FAILED, status);
  ASSERT_EQ(2u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(STOPPED, events_[1].type);
  EXPECT_EQ(EmbeddedWorkerInstance::STARTING, events_[1].status);
}

}  // namespace content
