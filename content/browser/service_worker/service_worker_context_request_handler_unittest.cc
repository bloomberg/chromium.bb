// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/fileapi/mock_url_request_delegate.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_request_handler.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/browser/service_worker/service_worker_write_to_cache_job.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

int kMockRenderProcessId = 1224;

void EmptyCallback() {}

}

class ServiceWorkerContextRequestHandlerTest : public testing::Test {
 public:
  ServiceWorkerContextRequestHandlerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() OVERRIDE {
    helper_.reset(new EmbeddedWorkerTestHelper(kMockRenderProcessId));

    // A new unstored registration/version.
    scope_ = GURL("http://host/scope/");
    script_url_ = GURL("http://host/script.js");
    registration_ = new ServiceWorkerRegistration(
        scope_, 1L, context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(), script_url_, 1L, context()->AsWeakPtr());

    // An empty host.
    scoped_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
        kMockRenderProcessId, 1 /* provider_id */,
        context()->AsWeakPtr(), NULL));
    provider_host_ = host->AsWeakPtr();
    context()->AddProviderHost(host.Pass());

    context()->storage()->LazyInitialize(base::Bind(&EmptyCallback));
    base::RunLoop().RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    version_ = NULL;
    registration_ = NULL;
    helper_.reset();
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  net::URLRequestContext url_request_context_;
  MockURLRequestDelegate url_request_delegate_;
  GURL scope_;
  GURL script_url_;
};

TEST_F(ServiceWorkerContextRequestHandlerTest, UpdateBefore24Hours) {
  // Give the registration a very recent last update time and pretend
  // we're installing a new version.
  registration_->set_last_update_check(base::Time::Now());
  version_->SetStatus(ServiceWorkerVersion::NEW);
  provider_host_->running_hosted_version_ = version_;

  // Conduct a resource fetch for the main script.
  const GURL kScriptUrl("http://host/script.js");
  scoped_ptr<net::URLRequest> request = url_request_context_.CreateRequest(
      kScriptUrl,
      net::DEFAULT_PRIORITY,
      &url_request_delegate_,
      NULL);
  scoped_ptr<ServiceWorkerContextRequestHandler> handler(
      new ServiceWorkerContextRequestHandler(
          context()->AsWeakPtr(),
          provider_host_,
          base::WeakPtr<storage::BlobStorageContext>(),
          RESOURCE_TYPE_SERVICE_WORKER));
  scoped_refptr<net::URLRequestJob> job =
      handler->MaybeCreateJob(request.get(), NULL);
  ASSERT_TRUE(job.get());
  ServiceWorkerWriteToCacheJob* sw_job =
      static_cast<ServiceWorkerWriteToCacheJob*>(job.get());

  // Verify the net request is not initialized to bypass the browser cache.
  EXPECT_FALSE(sw_job->net_request_->load_flags() & net::LOAD_BYPASS_CACHE);
}

TEST_F(ServiceWorkerContextRequestHandlerTest, UpdateAfter24Hours) {
  // Give the registration a old update time and pretend
  // we're installing a new version.
  registration_->set_last_update_check(
      base::Time::Now() - base::TimeDelta::FromDays(7));
  version_->SetStatus(ServiceWorkerVersion::NEW);
  provider_host_->running_hosted_version_ = version_;

  // Conduct a resource fetch for the main script.
  const GURL kScriptUrl("http://host/script.js");
  scoped_ptr<net::URLRequest> request = url_request_context_.CreateRequest(
      kScriptUrl,
      net::DEFAULT_PRIORITY,
      &url_request_delegate_,
      NULL);
  scoped_ptr<ServiceWorkerContextRequestHandler> handler(
      new ServiceWorkerContextRequestHandler(
          context()->AsWeakPtr(),
          provider_host_,
          base::WeakPtr<storage::BlobStorageContext>(),
          RESOURCE_TYPE_SERVICE_WORKER));
  scoped_refptr<net::URLRequestJob> job =
      handler->MaybeCreateJob(request.get(), NULL);
  ASSERT_TRUE(job.get());
  ServiceWorkerWriteToCacheJob* sw_job =
      static_cast<ServiceWorkerWriteToCacheJob*>(job.get());

  // Verify the net request is initialized to bypass the browser cache.
  EXPECT_TRUE(sw_job->net_request_->load_flags() & net::LOAD_BYPASS_CACHE);
}

}  // namespace content
