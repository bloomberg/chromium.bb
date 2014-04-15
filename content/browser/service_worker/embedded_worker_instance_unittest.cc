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
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

static const int kRenderProcessId = 11;

class EmbeddedWorkerInstanceTest : public testing::Test {
 protected:
  EmbeddedWorkerInstanceTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() OVERRIDE {
    context_.reset(new ServiceWorkerContextCore(base::FilePath(), NULL));
    helper_.reset(new EmbeddedWorkerTestHelper(
        context_.get(), kRenderProcessId));
  }

  virtual void TearDown() OVERRIDE {
    helper_.reset();
    context_.reset();
  }

  EmbeddedWorkerRegistry* embedded_worker_registry() {
    DCHECK(context_);
    return context_->embedded_worker_registry();
  }

  IPC::TestSink* ipc_sink() { return helper_->ipc_sink(); }

  TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<ServiceWorkerContextCore> context_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstanceTest);
};

TEST_F(EmbeddedWorkerInstanceTest, StartAndStop) {
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  const int embedded_worker_id = worker->embedded_worker_id();
  const int64 service_worker_version_id = 55L;
  const GURL scope("http://example.com/*");
  const GURL url("http://example.com/worker.js");

  // This fails as we have no available process yet.
  EXPECT_EQ(SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND,
            worker->Start(service_worker_version_id, scope, url));
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  // Simulate adding one process to the worker.
  helper_->SimulateAddProcessToWorker(embedded_worker_id, kRenderProcessId);

  // Start should succeed.
  EXPECT_EQ(SERVICE_WORKER_OK,
            worker->Start(service_worker_version_id, scope, url));
  EXPECT_EQ(EmbeddedWorkerInstance::STARTING, worker->status());
  base::RunLoop().RunUntilIdle();

  // Worker started message should be notified (by EmbeddedWorkerTestHelper).
  EXPECT_EQ(EmbeddedWorkerInstance::RUNNING, worker->status());
  EXPECT_EQ(kRenderProcessId, worker->process_id());

  // Stop the worker.
  EXPECT_EQ(SERVICE_WORKER_OK, worker->Stop());
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPING, worker->status());
  base::RunLoop().RunUntilIdle();

  // Worker stopped message should be notified (by EmbeddedWorkerTestHelper).
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  // Verify that we've sent two messages to start and terminate the worker.
  ASSERT_TRUE(ipc_sink()->GetUniqueMessageMatching(
      EmbeddedWorkerMsg_StartWorker::ID));
  ASSERT_TRUE(ipc_sink()->GetUniqueMessageMatching(
      EmbeddedWorkerMsg_StopWorker::ID));
}

TEST_F(EmbeddedWorkerInstanceTest, ChooseProcess) {
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  // Simulate adding processes to the worker.
  // Process 1 has 1 ref, 2 has 2 refs and 3 has 3 refs.
  const int embedded_worker_id = worker->embedded_worker_id();
  helper_->SimulateAddProcessToWorker(embedded_worker_id, 1);
  helper_->SimulateAddProcessToWorker(embedded_worker_id, 2);
  helper_->SimulateAddProcessToWorker(embedded_worker_id, 2);
  helper_->SimulateAddProcessToWorker(embedded_worker_id, 3);
  helper_->SimulateAddProcessToWorker(embedded_worker_id, 3);
  helper_->SimulateAddProcessToWorker(embedded_worker_id, 3);

  // Process 3 has the biggest # of references and it should be chosen.
  EXPECT_EQ(SERVICE_WORKER_OK,
            worker->Start(1L,
                          GURL("http://example.com/*"),
                          GURL("http://example.com/worker.js")));
  EXPECT_EQ(EmbeddedWorkerInstance::STARTING, worker->status());
  EXPECT_EQ(3, worker->process_id());

  // Wait until started message is sent back.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerInstance::RUNNING, worker->status());
}

}  // namespace content
