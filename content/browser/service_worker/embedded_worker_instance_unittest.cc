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

void SaveStatusAndCall(ServiceWorkerStatusCode* out,
                       const base::Closure& callback,
                       ServiceWorkerStatusCode status) {
  *out = status;
  callback.Run();
}

}  // namespace

class EmbeddedWorkerInstanceTest : public testing::Test {
 protected:
  EmbeddedWorkerInstanceTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(kRenderProcessId));
  }

  void TearDown() override { helper_.reset(); }

  ServiceWorkerStatusCode StartWorker(EmbeddedWorkerInstance* worker,
                                      int id, const GURL& pattern,
                                      const GURL& url) {
    ServiceWorkerStatusCode status;
    base::RunLoop run_loop;
    worker->Start(id, pattern, url, false,
                  base::Bind(&SaveStatusAndCall, &status,
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

 private:
  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstanceTest);
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
      false,
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
  ASSERT_TRUE(ipc_sink()->GetUniqueMessageMatching(
      EmbeddedWorkerMsg_StartWorker::ID));
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

TEST_F(EmbeddedWorkerInstanceTest, InstanceDestroyedBeforeStartFinishes) {
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  const int64 service_worker_version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");

  ServiceWorkerStatusCode status;
  base::RunLoop run_loop;
  // Begin starting the worker.
  context()->process_manager()->AddProcessReferenceToPattern(
      pattern, kRenderProcessId);
  worker->Start(
      service_worker_version_id,
      pattern,
      url,
      false,
      base::Bind(&SaveStatusAndCall, &status, run_loop.QuitClosure()));
  // But destroy it before it gets a chance to complete.
  worker.reset();
  run_loop.Run();
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT, status);

  // Verify that we didn't send the message to start the worker.
  ASSERT_FALSE(
      ipc_sink()->GetUniqueMessageMatching(EmbeddedWorkerMsg_StartWorker::ID));
}

}  // namespace content
