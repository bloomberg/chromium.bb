// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ServiceWorkerVersionTest : public testing::Test {
 protected:
  ServiceWorkerVersionTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() OVERRIDE {
    context_.reset(new ServiceWorkerContextCore(base::FilePath(), NULL));
    helper_.reset(new EmbeddedWorkerTestHelper(context_.get()));

    registration_ = new ServiceWorkerRegistration(
        GURL("http://www.example.com/*"),
        GURL("http://www.example.com/service_worker.js"),
        1L);
  }

  virtual void TearDown() OVERRIDE {
    registration_->Shutdown();
    helper_.reset();
    context_.reset();
  }

  EmbeddedWorkerRegistry* embedded_worker_registry() {
    return context_->embedded_worker_registry();
  }

  TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<ServiceWorkerContextCore> context_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerVersionTest);
};

TEST_F(ServiceWorkerVersionTest, ConcurrentStartAndStop) {
  const int64 version_id = 1L;

  scoped_refptr<ServiceWorkerVersion> version = new ServiceWorkerVersion(
      registration_,
      embedded_worker_registry(),
      version_id);
  int embedded_worker_id = version->embedded_worker()->embedded_worker_id();

  // Simulate adding one process to the worker.
  helper_->SimulateAddProcess(embedded_worker_id, 1);

  BrowserThread::ID current = BrowserThread::IO;

  // Call StartWorker() multiple times.
  ServiceWorkerStatusCode status1 = SERVICE_WORKER_ERROR_FAILED;
  ServiceWorkerStatusCode status2 = SERVICE_WORKER_ERROR_FAILED;
  ServiceWorkerStatusCode status3 = SERVICE_WORKER_ERROR_FAILED;
  version->StartWorker(CreateReceiver(current, base::Closure(), &status1));
  version->StartWorker(CreateReceiver(current, base::Closure(), &status2));

  EXPECT_EQ(ServiceWorkerVersion::STARTING, version->status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version->status());

  // Call StartWorker() after it's started.
  version->StartWorker(CreateReceiver(current, base::Closure(), &status3));
  base::RunLoop().RunUntilIdle();

  // All should just succeed.
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);
  EXPECT_EQ(SERVICE_WORKER_OK, status3);

  // Call StopWorker() multiple times.
  status1 = SERVICE_WORKER_ERROR_FAILED;
  status2 = SERVICE_WORKER_ERROR_FAILED;
  status3 = SERVICE_WORKER_ERROR_FAILED;
  version->StopWorker(CreateReceiver(current, base::Closure(), &status1));
  version->StopWorker(CreateReceiver(current, base::Closure(), &status2));

  // Also try calling StartWorker while StopWorker is in queue.
  version->StartWorker(CreateReceiver(current, base::Closure(), &status3));

  EXPECT_EQ(ServiceWorkerVersion::STOPPING, version->status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version->status());

  // All StopWorker should just succeed, while StartWorker fails.
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);
  EXPECT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, status3);

  version->Shutdown();
}

}  // namespace content
