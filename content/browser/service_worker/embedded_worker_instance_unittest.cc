// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const int kRenderProcessId = 11;

void DestroyWorker(scoped_ptr<EmbeddedWorkerInstance> worker,
                   ServiceWorkerStatusCode* out_status,
                   ServiceWorkerStatusCode status) {
  *out_status = status;
  worker.reset();
}

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

  void OnStopped(EmbeddedWorkerInstance::Status old_status) override {
    stopped_ = true;
    stopped_old_status_ = old_status;
  }

  void OnDetached(EmbeddedWorkerInstance::Status old_status) override {
    detached_ = true;
    detached_old_status_ = old_status;
  }

  bool OnMessageReceived(const IPC::Message& message) override { return false; }

  void SetUp() override {
    helper_.reset(
        new EmbeddedWorkerTestHelper(base::FilePath(), kRenderProcessId));
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
  bool detached_ = false;
  EmbeddedWorkerInstance::Status detached_old_status_ =
      EmbeddedWorkerInstance::STOPPED;
  bool stopped_ = false;
  EmbeddedWorkerInstance::Status stopped_old_status_ =
      EmbeddedWorkerInstance::STOPPED;

 private:
  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstanceTest);
};

class FailToSendIPCHelper : public EmbeddedWorkerTestHelper {
 public:
  FailToSendIPCHelper()
      : EmbeddedWorkerTestHelper(base::FilePath(), kRenderProcessId) {}
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

  const int64 service_worker_version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");

  // Simulate adding one process to the pattern.
  helper_->SimulateAddProcessToPattern(pattern, kRenderProcessId);

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
  EXPECT_EQ(kRenderProcessId, worker->process_id());

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

  const int64 service_worker_version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");

  // Simulate adding one process to the pattern.
  helper_->SimulateAddProcessToPattern(pattern, kRenderProcessId);

  // Start the worker and then call StopIfIdle().
  EXPECT_EQ(SERVICE_WORKER_OK,
            StartWorker(worker.get(), service_worker_version_id, pattern, url));
  EXPECT_EQ(EmbeddedWorkerInstance::RUNNING, worker->status());
  EXPECT_EQ(kRenderProcessId, worker->process_id());
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
  EXPECT_EQ(kRenderProcessId, worker->process_id());
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

  const int64 version_id1 = 55L;
  const int64 version_id2 = 56L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");

  helper_->SimulateAddProcessToPattern(pattern, kRenderProcessId);
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
  EXPECT_EQ(0UL,
            registry->worker_process_map_[kRenderProcessId].count(worker1_id));
  EXPECT_EQ(1UL, registry->worker_process_map_[kRenderProcessId].count(
                     worker2->embedded_worker_id()));

  worker2->Stop();
}

// Test detaching in the middle of the start worker sequence.
TEST_F(EmbeddedWorkerInstanceTest, DetachDuringStart) {
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  worker->AddListener(this);

  scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params(
      new EmbeddedWorkerMsg_StartWorker_Params());
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;

  // Pretend the worker got stopped before the start sequence reached
  // SendStartWorker.
  worker->status_ = EmbeddedWorkerInstance::STOPPED;
  base::RunLoop run_loop;
  worker->SendStartWorker(params.Pass(), base::Bind(&SaveStatusAndCall, &status,
                                                    run_loop.QuitClosure()),
                          true, -1, false);
  run_loop.Run();
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT, status);
  // Don't expect SendStartWorker() to dispatch an OnStopped/Detached() message
  // since the worker was already stopped.
  EXPECT_FALSE(stopped_);
  EXPECT_FALSE(detached_);

  // Repeat, this time have the start callback destroy the worker, as is
  // usual when starting a worker fails, and ensure a crash doesn't occur.
  worker->status_ = EmbeddedWorkerInstance::STOPPED;
  EmbeddedWorkerInstance* worker_ptr = worker.get();
  worker_ptr->SendStartWorker(
      params.Pass(), base::Bind(&DestroyWorker, base::Passed(&worker), &status),
      true, -1, false);
  // No crash.
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT, status);
}

// Test stopping in the middle of the start worker sequence, before
// a process is allocated.
TEST_F(EmbeddedWorkerInstanceTest, StopDuringStart) {
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  worker->AddListener(this);
  // Pretend we stop during starting before we got a process allocated.
  worker->status_ = EmbeddedWorkerInstance::STARTING;
  worker->process_id_ = -1;
  worker->Stop();
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());
  EXPECT_TRUE(detached_);
  EXPECT_EQ(EmbeddedWorkerInstance::STARTING, detached_old_status_);
}

TEST_F(EmbeddedWorkerInstanceTest, Detach) {
  const int64 version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  helper_->SimulateAddProcessToPattern(pattern, kRenderProcessId);
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

  const int64 version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");

  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  helper_->SimulateAddProcessToPattern(pattern, kRenderProcessId);
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
  EXPECT_TRUE(stopped_);
  EXPECT_EQ(EmbeddedWorkerInstance::STARTING, stopped_old_status_);
}

}  // namespace content
